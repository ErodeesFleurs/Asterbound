module;
extern "C" {
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}
import star.json;

import std;
import star.type_identity;

import star.lexical_cast;
import star.string;
import star.ref_ptr;
import star.list;
import star.map;
import star.set;
import star.optional;
import star.format;
import star.text_formatter;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;

import star.byte_array;
import star.algorithm;
namespace Star {

class LuaEngine;
class Image;

// Engine-owned host state keeps domain capabilities alive with Lua references.
class LuaExecutionHost {
public:
  virtual ~LuaExecutionHost() = default;
  virtual void query_type(TypeQuery& query) const noexcept { query_type_of<LuaExecutionHost>(*this, query); }
  virtual std::string_view dynamic_type_name() const noexcept { return type_name<LuaExecutionHost>(); }
};
using LuaEnginePtr = RefPtr<LuaEngine>;

// General Lua diagnostic category.
struct LuaExceptionTag {
  static constexpr std::string_view name() { return "LuaException"; }
};
using LuaException = StarError<LuaExceptionTag, StarException>;

// Explicit host infrastructure failures keep their native identity across Lua.
struct LuaHostErrorTag {
  static constexpr std::string_view name() { return "LuaHostError"; }
};
using LuaHostError = StarError<LuaHostErrorTag, StarException>;

// Incomplete statement diagnostics let REPL callers request more input.
// Lua uses the trailing '<eof>' marker for this category.
struct LuaIncompleteStatementExceptionTag {
  static constexpr std::string_view name() { return "LuaIncompleteStatementException"; }
};
using LuaIncompleteStatementException = StarError<LuaIncompleteStatementExceptionTag, LuaException>;

// Returned when an enabled instruction limit is reached.
struct LuaInstructionLimitReachedTag {
  static constexpr std::string_view name() { return "LuaInstructionLimitReached"; }
};
using LuaInstructionLimitReached = StarError<LuaInstructionLimitReachedTag, LuaException>;

// Returned when an enabled engine recursion limit is reached.
struct LuaRecursionLimitReachedTag {
  static constexpr std::string_view name() { return "LuaRecursionLimitReached"; }
};
using LuaRecursionLimitReached = StarError<LuaRecursionLimitReachedTag, LuaException>;

// Returned when a Lua value cannot convert to the requested C++ type.
struct LuaConversionExceptionTag {
  static constexpr std::string_view name() { return "LuaConversionException"; }
};
using LuaConversionException = StarError<LuaConversionExceptionTag, LuaException>;

using LuaNilType = std::monostate;
using LuaBoolean = bool;
using LuaInt = lua_Integer;
using LuaFloat = lua_Number;
class LuaString;
class LuaTable;
class LuaFunction;
class LuaThread;
class LuaUserData;
using LuaValue =
  std::variant<LuaNilType, LuaBoolean, LuaInt, LuaFloat, LuaString, LuaTable, LuaFunction, LuaThread, LuaUserData>;

// Used to wrap multiple return values from calling a lua function or to pass
// multiple values as arguments to a lua function from a container.  If this is
// used as an argument to a lua callback function, it must be the final
// argument of the function!
template <typename T>
// A project sequence tag: Lua codecs expand these values into distinct stack slots.
class LuaVariadic : public List<T> {
public:
  using List<T>::List;
  LuaVariadic() = default;
  LuaVariadic(List<T> const& values) : List<T>(values) {}
  LuaVariadic(List<T>&& values) : List<T>(std::move(values)) {}
};

// Unpack a container and apply each of the arguments separately to a lua
// function, similar to lua's unpack.
template <typename Container> LuaVariadic<typename std::decay_t<Container>::value_type> lua_unpack(Container&& c);

// Similar to LuaVariadic, but a tuple type so automatic per-entry type
// conversion is done.  This can only be used as the return value of a wrapped
// c++ function, or as a type for the return value of calling a lua function.
template <typename... Types>
class LuaTupleReturn : public std::tuple<Types...> {
public:
  using Base = std::tuple<Types...>;

  explicit LuaTupleReturn(Types const&... args);
  template <typename... UTypes>
    requires std::constructible_from<std::tuple<Types...>, UTypes&&...>
  explicit LuaTupleReturn(UTypes&&... args);
  template <typename... UTypes>
  explicit LuaTupleReturn(UTypes const&... args);
  LuaTupleReturn(LuaTupleReturn const& rhs);
  LuaTupleReturn(LuaTupleReturn&& rhs);
  template <typename... UTypes>
  LuaTupleReturn(LuaTupleReturn<UTypes...> const& rhs);
  template <typename... UTypes>
  LuaTupleReturn(LuaTupleReturn<UTypes...>&& rhs);

  LuaTupleReturn& operator=(LuaTupleReturn const& rhs);
  LuaTupleReturn& operator=(LuaTupleReturn&& rhs);
  template <typename... UTypes>
  LuaTupleReturn& operator=(LuaTupleReturn<UTypes...> const& rhs);
  template <typename... UTypes>
  LuaTupleReturn& operator=(LuaTupleReturn<UTypes...>&& rhs);
};

// std::tie for LuaTupleReturn
template <typename... Types>
LuaTupleReturn<Types&...> lua_tie(Types&... args);

// Constructs a LuaTupleReturn from the given arguments similar to make_tuple
template <typename... Types> LuaTupleReturn<std::decay_t<Types>...> lua_tuple_return(Types&&... args);

// Synchronous stack borrows. Views never escape their call; use own() when a
// value must survive it. All allocating/metamethod operations stay protected.
class LuaCallbacks;
class LuaStackValue;
class LuaStack {
public:
  LuaStack(LuaEngine& engine, lua_State& state) : m_engine(engine), m_state(state) {}
  LuaEngine& engine() const { return m_engine; }
  lua_State& state() const { return m_state; }
  int top() const { return lua_gettop(&m_state); }
  void pop(int count = 1) const { lua_pop(&m_state, count); }
  LuaStackValue value(int index) const;
  template <typename T> [[nodiscard]] std::expected<void, StarException> push(T&& value) const;
  template <typename T> [[nodiscard]] std::expected<T, StarException> read(int index) const;
  template <typename T> [[nodiscard]] std::expected<std::optional<T>, StarException> maybe_read(int index) const;
  [[nodiscard]] std::expected<LuaStackValue, StarException> create_table(int narr = 0, int nrec = 0) const;
  [[nodiscard]] std::expected<void, StarException> push_callback_table(LuaCallbacks const& callbacks) const;
  template <typename Container>
  [[nodiscard]] std::expected<void, StarException> push_array(Container const& values) const;
  template <typename Container> [[nodiscard]] std::expected<void, StarException> push_map(Container const& values) const;
  [[nodiscard]] std::expected<void, StarException> push_string(std::string_view value) const;
  template <typename T> [[nodiscard]] std::expected<void, StarException> push_userdata(T value) const;
  [[nodiscard]] std::expected<void, StarException> call(lua_CFunction function, int nargs, int nresults) const;
  // Execute source or bytecode in a borrowed environment on this same stack.
  [[nodiscard]] std::expected<void, StarException> load(LuaStackValue environment, std::span<char const> contents,
                                                        String const& name = {}) const;

private:
  LuaEngine& m_engine;
  lua_State& m_state;
};

class LuaStackValue {
public:
  LuaStackValue(LuaStack stack, int index) : m_stack(stack), m_index(lua_absindex(&stack.state(), index)) {}
  LuaStack stack() const { return m_stack; }
  int index() const { return m_index; }
  template <typename T> bool is() const;
  template <typename T> T as() const;
  StringView string_view() const;
  String to_string() const { return String(string_view().utf8()); }
  template <typename T> [[nodiscard]] std::expected<T, StarException> to() const;
  template <typename T> [[nodiscard]] std::expected<std::optional<T>, StarException> maybe_to() const;
  template <typename T = LuaValue, typename K> [[nodiscard]] std::expected<T, StarException> get(K const& key) const;
  template <typename T, typename K>
  [[nodiscard]] std::expected<std::optional<T>, StarException> maybe_get(K const& key) const;
  template <typename K> [[nodiscard]] std::expected<bool, StarException> contains(K const& key) const;
  template <typename T = LuaValue, typename K> [[nodiscard]] std::expected<T, StarException> raw_get(K const& key) const;
  template <typename K, typename V> [[nodiscard]] std::expected<void, StarException> set(K const& key, V&& value) const;
  template <typename K, typename V>
  [[nodiscard]] std::expected<void, StarException> raw_set(K const& key, V&& value) const;
  template <typename Function> [[nodiscard]] std::expected<void, StarException> iterate(Function&& function) const;
  [[nodiscard]] std::expected<LuaInt, StarException> length() const;
  [[nodiscard]] std::expected<LuaInt, StarException> raw_length() const;
  [[nodiscard]] std::expected<LuaValue, StarException> own() const;
  template <typename T> [[nodiscard]] std::expected<bool, StarException> is_userdata() const;
  template <typename T> [[nodiscard]] std::expected<std::reference_wrapper<T>, StarException> userdata() const;
  [[nodiscard]] std::expected<void, StarException> validate() const;

private:
  template <typename K> [[nodiscard]] std::expected<void, StarException> push_field(K const& key, bool raw) const;
  template <typename K, typename V>
  [[nodiscard]] std::expected<void, StarException> set_field(K const& key, V&& value, bool raw) const;
  LuaStack m_stack;
  int m_index;
};

namespace LuaDetail {
// Only signature-deducing entrypoints require metadata; generic and overloaded
// callables can use the explicit-signature APIs without probing their bodies.
template <typename Function>
concept signature_deducible = requires {
  typename FunctionTraits<Function>::result_type;
  typename FunctionTraits<Function>::argument_types;
};

[[nodiscard]] std::expected<void, StarException> insert_json_metatable(LuaEngine& engine, LuaTable const& table,
                                                                     Json::Type type);
struct LuaRegistryReference {
  LuaRegistryReference(LuaEnginePtr engine, int handle_index);
  LuaRegistryReference(LuaRegistryReference const&) = delete;
  LuaRegistryReference& operator=(LuaRegistryReference const&) = delete;
  ~LuaRegistryReference();

  LuaEnginePtr engine;
  int handle_index;
};

  struct LuaHandle {
    LuaHandle(LuaEnginePtr engine, int handle_index);
    ~LuaHandle() = default;

    LuaHandle(LuaHandle const&) = default;
    LuaHandle(LuaHandle&&) = default;
    LuaHandle& operator=(LuaHandle const&) = default;
    LuaHandle& operator=(LuaHandle&&) = default;

    std::shared_ptr<LuaRegistryReference> reference;
  };

  // Wrapped callbacks decode borrowed arguments and append their results.
  using LuaWrappedFunction = std::function<std::expected<int, StarException>(LuaStack)>;
}

// Rendering may inspect tables and therefore reports recoverable Lua errors.
[[nodiscard]] std::expected<void, StarException> write_text(TextBuffer& output, LuaValue const& value);
[[nodiscard]] std::expected<String, StarException> print_lua_value(LuaValue const& value);

// Holds a reference to a LuaEngine and a value held internally inside the
// registry of that engine.  The lifetime of the LuaEngine will be extended
// until all LuaReferences referencing it are destroyed.
class LuaReference {
public:
  LuaReference(LuaDetail::LuaHandle handle);

  LuaReference(LuaReference&&) = default;
  LuaReference& operator=(LuaReference&&) = default;

  LuaReference(LuaReference const&) = default;
  LuaReference& operator=(LuaReference const&) = default;

  bool operator==(LuaReference const& rhs) const;

  LuaEngine& engine() const;
  int handle_index() const;

private:
  LuaDetail::LuaHandle m_handle;
};

class LuaString : public LuaReference {
public:
  using LuaReference::LuaReference;

  std::size_t length() const;

  String to_string() const;
  StringView view() const;
};

bool operator==(LuaString const& s1, LuaString const& s2);
bool operator==(LuaString const& s1, std::string_view s2);
bool operator==(LuaString const& s1, std::string const& s2);
bool operator==(LuaString const& s1, String const& s2);
bool operator==(std::string_view s1, LuaString const& s2);
template <std::size_t extent>
bool operator==(LuaString const& s1, char const (&s2)[extent]) { return s1 == std::string_view(s2); }
template <std::size_t extent>
bool operator==(char const (&s1)[extent], LuaString const& s2) { return std::string_view(s1) == s2; }
bool operator==(std::string const& s1, LuaString const& s2);
bool operator==(String const& s1, LuaString const& s2);

// C++20 rewrites != from these.
bool operator==(LuaString const& s1, LuaString const& s2);

class LuaTable : public LuaReference {
public:
  using LuaReference::LuaReference;

  template <typename T = LuaValue, typename K>
  [[nodiscard]] std::expected<T, StarException> get(K const& key) const;
  template <typename T = LuaValue>
  [[nodiscard]] std::expected<T, StarException> get(std::string_view key) const;

  template <typename T, typename K>
  [[nodiscard]] std::expected<void, StarException> set(K const& key, T&& t) const;
  template <typename T>
  [[nodiscard]] std::expected<void, StarException> set(std::string_view key, T&& t) const;

  // Shorthand for get(path) != lua_nil
  template <typename K>
  [[nodiscard]] std::expected<bool, StarException> contains(K const& key) const;
  [[nodiscard]] std::expected<bool, StarException> contains(std::string_view key) const;

  // Shorthand for setting to lua_nil
  template <typename K>
  [[nodiscard]] std::expected<void, StarException> remove(K const& key) const;
  [[nodiscard]] std::expected<void, StarException> remove(std::string_view key) const;

  // Result of lua # operator
  [[nodiscard]] std::expected<LuaInt, StarException> length() const;

  // If iteration function returns bool, returning false signals stopping.
  template <LuaDetail::signature_deducible Function>
  [[nodiscard]] std::expected<void, StarException> iterate(Function&& iterator) const;

  template <typename Return, typename... Args, typename Function>
  [[nodiscard]] std::expected<void, StarException> iterate_with_signature(Function&& func) const;

  [[nodiscard]] std::expected<std::optional<LuaTable>, StarException> get_metatable() const;
  [[nodiscard]] std::expected<void, StarException> set_metatable(LuaTable const& table) const;

  template <typename T = LuaValue, typename K>
  [[nodiscard]] std::expected<T, StarException> raw_get(K const& key) const;
  template <typename T = LuaValue>
  [[nodiscard]] std::expected<T, StarException> raw_get(std::string_view key) const;

  template <typename T, typename K>
  [[nodiscard]] std::expected<void, StarException> raw_set(K const& key, T&& t) const;
  template <typename T>
  [[nodiscard]] std::expected<void, StarException> raw_set(std::string_view key, T&& t) const;

  [[nodiscard]] std::expected<LuaInt, StarException> raw_length() const;
};

class LuaFunction : public LuaReference {
public:
  using LuaReference::LuaReference;

  template <typename Ret = LuaValue, typename... Args>
  [[nodiscard]] std::expected<Ret, StarException> invoke(Args const&... args) const;
};

class LuaThread : public LuaReference {
public:
  using LuaReference::LuaReference;
  enum class Status {
    Dead,
    Active,
    Error
  };

  // Will return a value if the thread has yielded a value, and nothing if the
  // thread has finished execution
  template <typename Ret = LuaValue, typename... Args>
  [[nodiscard]] std::expected<std::optional<Ret>, StarException> resume(Args const&... args) const;
  [[nodiscard]] std::expected<void, StarException> push_function(LuaFunction const& func) const;
  Status status() const;
};

// Keeping LuaReferences in LuaUserData will lead to circular references to
// LuaEngine, in addition to circular references in Lua which the Lua
// garbage collector can't collect. Don't put LuaReferences in LuaUserData.
class LuaUserData : public LuaReference {
public:
  using LuaReference::LuaReference;

  template <typename T>
  [[nodiscard]] std::expected<bool, StarException> is() const;

  template <typename T>
  [[nodiscard]] std::expected<std::reference_wrapper<T>, StarException> get() const;
};

inline const LuaValue lua_nil = LuaValue();

class LuaCallbacks {
public:
  [[nodiscard]] std::expected<void, StarException> copy_callback(String src_name, String dst_name);

  template <LuaDetail::signature_deducible Function>
  [[nodiscard]] std::expected<void, StarException> register_callback(String name, Function&& func);

  bool remove_callback(String name);

  template <typename Return, typename... Args, typename Function>
  [[nodiscard]] std::expected<void, StarException> register_callback_with_signature(String name, Function&& func);

  [[nodiscard]] std::expected<void, StarException> merge(LuaCallbacks const& callbacks);

  // Borrowed callbacks installed by an owner may outlive its Lua context.
  // Retired scopes reject even saved Lua function values before dereferencing.
  LuaCallbacks with_lifetime(std::stop_token lifetime) const;

  StringMap<LuaDetail::LuaWrappedFunction> const& callbacks() const;

private:
  StringMap<LuaDetail::LuaWrappedFunction> m_callbacks;
};

template <typename T>
class LuaMethods {
public:
  template <LuaDetail::signature_deducible Function>
  [[nodiscard]] std::expected<void, StarException> register_method(String name, Function&& func);

  template <typename Return, typename... Args, typename Function>
  [[nodiscard]] std::expected<void, StarException> register_method_with_signature(String name, Function&& func);

  StringMap<LuaDetail::LuaWrappedFunction> const& methods() const;

private:
  StringMap<LuaDetail::LuaWrappedFunction> m_methods;
};

// Types that want to participate in automatic lua conversion should specialize
// this template and provide static to and from methods on it.  The method
// signatures will be called like:
//   std::expected<void, StarException> from(LuaStack stack, T t);
//   std::expected<std::optional<T>, StarException> to(LuaStackValue value);
// A successful disengaged optional means a type mismatch; an unexpected value
// is an operational failure. Const-reference parameters and unconditionally
// convertible expected<T, StarException> results are also supported.
template <typename T>
struct LuaConverter;

namespace LuaDetail {
  template <typename T>
  concept lua_pushable = requires(LuaStack const& stack, T&& value) {
    { LuaConverter<std::remove_cvref_t<T>>::from(stack, std::forward<T>(value)) }
      -> std::convertible_to<std::expected<void, StarException>>;
  };

  template <typename T>
  concept lua_readable = std::is_object_v<T> && requires(LuaStackValue value) { LuaConverter<T>::to(value); };

  template <typename T>
  concept lua_call_argument = lua_pushable<T const&> || requires {
    requires std::same_as<T, LuaVariadic<typename T::value_type>>;
    requires lua_pushable<typename T::value_type const&>;
  };

  template <typename T> struct LuaReturn;
  template <typename T>
  concept lua_call_result = !std::is_reference_v<T> && requires(LuaStack stack) {
    { LuaReturn<T>::read(stack, 1, 1) } -> std::same_as<std::expected<T, StarException>>;
  };
}

// A single execution context from a LuaEngine that manages a (mostly) distinct
// lua environment.  Each LuaContext's global environment is separate and one
// LuaContext can (mostly) not affect any other.
class LuaContext : protected LuaTable {
public:
  using RequireFunction = std::function<std::expected<void, StarException>(LuaStackValue, StringView)>;

  using LuaTable::LuaTable;

  using LuaTable::get;
  using LuaTable::set;
  using LuaTable::contains;
  using LuaTable::remove;
  using LuaTable::engine;
  using LuaTable::handle_index;

  // Dot-separated byte keys (including empty keys). Reading through a
  // non-table yields nil; writing through nil creates the missing table.
  template <LuaDetail::lua_readable T = LuaValue>
  [[nodiscard]] std::expected<T, StarException> get_path(std::string_view path) const;
  [[nodiscard]] std::expected<bool, StarException> contains_path(std::string_view path) const;
  template <LuaDetail::lua_pushable T>
  [[nodiscard]] std::expected<void, StarException> set_path(std::string_view path, T&& value);

  // Load the given code (either source or bytecode) into this context as a new
  // chunk.  It is not necessary to provide the name again if given bytecode.
  [[nodiscard]] std::expected<void, StarException> load(std::span<char const> contents, String const& name = {});
  template <std::size_t extent>
  [[nodiscard]] std::expected<void, StarException> load(char const (&contents)[extent], String const& name = {}) {
    return load(std::span(contents).first(extent - 1), name);
  }
  [[nodiscard]] std::expected<void, StarException> load(String const& contents, String const& name = String());
  [[nodiscard]] std::expected<void, StarException> load(ByteArray const& contents, String const& name = String());

  // Evaluate a piece of lua code in this context, similar to the lua repl.
  // Can evaluate both expressions and statements.
  template <typename T = LuaValue>
  [[nodiscard]] std::expected<T, StarException> eval(String const& lua);

  // Override require with a synchronous callback borrowing its environment and
  // module name from the calling Lua stack.
  [[nodiscard]] std::expected<void, StarException> set_require_function(RequireFunction require_function);

  [[nodiscard]] std::expected<void, StarException> set_callbacks(String const& table_name, LuaCallbacks const& callbacks) const;

  // Resolve at every call, without promoting the function to a registry root.

  template <LuaDetail::lua_call_result Ret = LuaValue, LuaDetail::lua_call_argument... Args>
  [[nodiscard]] std::expected<Ret, StarException> invoke_path(std::string_view path, Args const&... args) const;
  template <LuaDetail::lua_call_result Ret = LuaValue, LuaDetail::lua_call_argument... Args>
    requires (!std::is_void_v<Ret>)
  [[nodiscard]] std::expected<std::optional<Ret>, StarException> invoke_path_if_present(
    std::string_view path, Args const&... args) const;

  // For convenience, calls to LuaEngine conversion / create functions are
  // duplicated here.

  template <typename T>
  [[nodiscard]] std::expected<LuaValue, StarException> lua_from(T&& t);
  template <typename T>
  [[nodiscard]] std::expected<LuaValue, StarException> lua_from(T const& t);
  template <typename T>
  [[nodiscard]] std::expected<std::optional<T>, StarException> lua_maybe_to(LuaValue&& v);
  template <typename T>
  [[nodiscard]] std::expected<std::optional<T>, StarException> lua_maybe_to(LuaValue const& v);
  template <typename T>
  [[nodiscard]] std::expected<T, StarException> lua_to(LuaValue const& v);
  template <typename T>
  [[nodiscard]] std::expected<T, StarException> lua_to(LuaValue&& v);

  [[nodiscard]] std::expected<LuaString, StarException> create_string(String const& str);
  [[nodiscard]] std::expected<LuaString, StarException> create_string(std::string_view str);
  [[nodiscard]] std::expected<LuaString, StarException> create_string(std::string const& str) { return create_string(std::string_view(str)); }
  template <std::size_t extent>
  [[nodiscard]] std::expected<LuaString, StarException> create_string(char const (&str)[extent]) { return create_string(std::string_view(str)); }

  [[nodiscard]] std::expected<LuaTable, StarException> create_table();

  template <typename Container>
  [[nodiscard]] std::expected<LuaTable, StarException> create_table(Container const& map);

  template <typename Container>
  [[nodiscard]] std::expected<LuaTable, StarException> create_array_table(Container const& array);

  template <LuaDetail::signature_deducible Function>
  [[nodiscard]] std::expected<LuaFunction, StarException> create_function(Function&& func);

  template <typename Return, typename... Args, typename Function>
  [[nodiscard]] std::expected<LuaFunction, StarException> create_function_with_signature(Function&& func);

  template <typename T>
  [[nodiscard]] std::expected<LuaUserData, StarException> create_userdata(T t);
};

// UserData types that want to expose methods to lua should specialize this
// template.
template <typename T>
struct LuaUserDataMethods {
  [[nodiscard]] static std::expected<LuaMethods<T>, StarException> make();
};

// Convenience converter that simply converts to/from LuaUserData, can be
// derived from by a declared converter.
template <typename T>
struct LuaUserDataConverter {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, T t);
  [[nodiscard]] static std::expected<std::optional<T>, StarException> to(LuaStackValue v);
};

struct LuaProfileEntry {
  // Source name of the chunk the function was defined in
  String source;
  // Line number in the chunk of the beginning of the function definition
  unsigned source_line;
  // Name of the function, if it can be determined
  std::optional<String> name;
  // Scope of the function, if it can be determined
  std::optional<String> name_scope;
  // Time taken within this function itself
  std::int64_t self_time;
  // Total time taken within this function or sub functions
  std::int64_t total_time;
  // Calls from this function
  HashMap<std::tuple<String, unsigned>, std::shared_ptr<LuaProfileEntry>> calls;
};

// This class represents one execution engine in lua, holding a single
// lua_State.  Multiple contexts can be created, and they will have separate
// global environments and cannot affect each other.  Individual LuaEngines /
// LuaContexts are not thread safe, use one LuaEngine per thread.
class LuaEngine : public RefCounter {
public:
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<LuaEngine, RefCounter>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return type_name<LuaEngine>(); }
  // If 'safe' is true, then creates a lua engine with all builtin lua
  // functions that can affect the real world disabled.
  [[nodiscard]] static std::expected<LuaEnginePtr, StarException> create(bool safe = true);

  ~LuaEngine() override;

  // Optional host capability. Core-only engines leave image resolution unset.
  using ImageResolver = std::function<std::expected<std::shared_ptr<Image const>, StarException>(String const&)>;
  void set_image_resolver(ImageResolver resolver) { m_image_resolver = std::move(resolver); }
  ImageResolver const& image_resolver() const { return m_image_resolver; }
  void set_execution_host(std::unique_ptr<LuaExecutionHost> host) { m_execution_host = std::move(host); }
  std::optional<std::reference_wrapper<LuaExecutionHost>> execution_host() const { return optional_ref(m_execution_host); }
  std::stop_token const& promise_lifetime() const { return m_promise_lifetime; }
  std::stop_token set_promise_lifetime(std::stop_token lifetime) {
    return std::exchange(m_promise_lifetime, std::move(lifetime));
  }

  // Set the instruction limit for computation sequences in the engine.  During
  // any function invocation, thread resume, or code evaluation, an instruction
  // counter will be started.  In the event that the instruction counter
  // becomes greater than the given limit, LuaInstructionLimitReached is returned. The
  // count is only reset when the initial entry into LuaEngine is returned,
  // recursive entries into LuaEngine accumulate the same instruction counter.
  // 0 disables the instruction limit.
  void set_instruction_limit(std::uint64_t instruction_limit = 0);
  std::uint64_t instruction_limit() const;

  // If profiling is enabled, then every 'measure_interval' instructions, the
  // function call stack will be recorded, and a summary of function timing can
  // be printed using get_profile
  void set_profiling_enabled(bool profiling_enabled);
  bool profiling_enabled() const;

  // Print a summary of the profiling data gathered since profiling was last
  // enabled.
  List<LuaProfileEntry> get_profile();

  // If an instruction limit is set or profiling is neabled, this field
  // describes the resolution of instruction count measurement, and affects the
  // accuracy of profiling and the instruction count limit.  Defaults to 1000
  void set_instruction_measure_interval(unsigned measure_interval = 1000);
  unsigned instruction_measure_interval() const;

  // Sets the LuaEngine recursion limit, limiting the number of times a
  // LuaEngine call may directly or inderectly trigger a call back into the
  // LuaEngine, preventing a C++ stack overflow.  0 disables the limit.
  void set_recursion_limit(unsigned recursion_limit = 0);
  unsigned recursion_limit() const;

  // Compile a given script into bytecode.  If name is given, then it will be
  // used as the internal name for the resulting chunk and will provide better
  // error messages.
  //
  // Unfortunately the only way to completely ensure that a single script will
  // execute in two separate contexts and truly be isolated is to compile the
  // script to bytecode and load once in each context as a separate chunk.
  [[nodiscard]] std::expected<ByteArray, StarException> compile(std::span<char const> contents, String const& name = {});
  template <std::size_t extent>
  [[nodiscard]] std::expected<ByteArray, StarException> compile(char const (&contents)[extent], String const& name = {}) {
    return compile(std::span(contents).first(extent - 1), name);
  }
  [[nodiscard]] std::expected<ByteArray, StarException> compile(String const& contents, String const& name = String());
  [[nodiscard]] std::expected<ByteArray, StarException> compile(ByteArray const& contents, String const& name = String());

  // Optional host diagnostics for full Lua chunk sources. The resolver must
  // own its snapshots, not borrow a shorter-lived host or capture this engine.
  // Failures are ignored so diagnostics cannot replace a script error.
  using SourceResolver = std::function<std::expected<String, StarException>(String const&)>;
  void set_source_resolver(SourceResolver resolver);

  // Returns the debug info of the state.
  lua_Debug const& debug_info(int level = 1, String const& what = "nSlu");
  // Owned source of an existing Lua frame; no Lua C API types escape.
  std::optional<String> script_source(int level = 1);

  // Generic from/to lua conversion, calls template specialization of
  // LuaConverter for actual conversion.
  template <typename T>
  [[nodiscard]] std::expected<LuaValue, StarException> lua_from(T&& t);
  template <typename T>
  [[nodiscard]] std::expected<LuaValue, StarException> lua_from(T const& t);
  template <typename T>
  [[nodiscard]] std::expected<std::optional<T>, StarException> lua_maybe_to(LuaValue&& v);
  template <typename T>
  [[nodiscard]] std::expected<std::optional<T>, StarException> lua_maybe_to(LuaValue const& v);

  // Wraps lua_maybe_to, reporting a conversion error if the value has no match.
  template <typename T>
  [[nodiscard]] std::expected<T, StarException> lua_to(LuaValue const& v);
  template <typename T>
  [[nodiscard]] std::expected<T, StarException> lua_to(LuaValue&& v);

  [[nodiscard]] std::expected<LuaString, StarException> create_string(String const& str);
  [[nodiscard]] std::expected<LuaString, StarException> create_string(std::string_view str);
  [[nodiscard]] std::expected<LuaString, StarException> create_string(std::string const& str) { return create_string(std::string_view(str)); }
  template <std::size_t extent>
  [[nodiscard]] std::expected<LuaString, StarException> create_string(char const (&str)[extent]) { return create_string(std::string_view(str)); }

  [[nodiscard]] std::expected<LuaTable, StarException> create_table(int narr = 0, int nrec = 0);

  template <typename Container>
  [[nodiscard]] std::expected<LuaTable, StarException> create_table(Container const& map);

  template <typename Container>
  [[nodiscard]] std::expected<LuaTable, StarException> create_array_table(Container const& array);

  // Creates a function and deduces the signature of the function using
  // FunctionTraits. As a convenience, the given function may optionally take
  // a LuaEngine& parameter as the first parameter, and if it does, when called
  // the function will get a reference to the calling LuaEngine.
  template <LuaDetail::signature_deducible Function>
  [[nodiscard]] std::expected<LuaFunction, StarException> create_function(Function&& func);

  // If the signature is not deducible using FunctionTraits (for example, a
  // generic or overloaded callable), specify the return and argument types
  // manually with create_function_with_signature.
  template <typename Return, typename... Args, typename Function>
  [[nodiscard]] std::expected<LuaFunction, StarException> create_function_with_signature(Function&& func);

  [[nodiscard]] std::expected<LuaFunction, StarException> create_wrapped_function(LuaDetail::LuaWrappedFunction function);

  [[nodiscard]] std::expected<LuaFunction, StarException> create_raw_function(lua_CFunction func);

  [[nodiscard]] std::expected<LuaThread, StarException> create_thread();

  template <typename T>
  [[nodiscard]] std::expected<LuaUserData, StarException> create_userdata(T t);

  [[nodiscard]] std::expected<LuaContext, StarException> create_context();

  // Global environment changes only affect newly created contexts

  template <typename T = LuaValue, typename K>
  [[nodiscard]] std::expected<T, StarException> get_global(K const& key);
  template <typename T = LuaValue>
  [[nodiscard]] std::expected<T, StarException> get_global(std::string_view key);

  template <typename T, typename K>
  [[nodiscard]] std::expected<void, StarException> set_global(K const& key, T&& value);

  template <typename T>
  [[nodiscard]] std::expected<void, StarException> set_global(std::string_view key, T&& value);

  // Full collection, or incremental work measured in KiB. Expired native
  // references are also released at ordinary call/resume boundaries.
  [[nodiscard]] std::expected<void, StarException> collect_garbage(std::optional<unsigned> steps = {});

  // Stop / start automatic garbage collection
  void set_auto_garbage_collection(bool enabled);

  // Tune the pause and step values of the lua garbage collector
  void tune_auto_garbage_collection(float pause, float step_multiplier);

  // Bytes in use by lua
  std::size_t memory_usage() const;

  [[nodiscard]] std::expected<void, StarException> add_imgui();
private:
  friend class LuaStack;
  friend class LuaStackValue;
  template <typename T> friend struct LuaConverter;
  friend struct LuaDetail::LuaRegistryReference;
  friend class LuaReference;
  friend class LuaString;
  friend class LuaTable;
  friend class LuaFunction;
  friend class LuaThread;
  friend class LuaUserData;
  friend class LuaContext;
  friend std::expected<void, StarException> LuaDetail::insert_json_metatable(LuaEngine& engine, LuaTable const& table,
                                                                           Json::Type type);

  LuaEngine() = default;

  // Recover the required engine capability from Lua's native extraspace.
  static LuaEngine& lua_engine(lua_State& state);
  static int context_loadstring(lua_State* state);
  template <typename T> static auto conversion_error() {
    return LuaConversionException::format("Error converting LuaValue to type '{}'", type_name<T>());
  }
  // Counts instructions when instruction limiting is enabled.
  static void count_hook(lua_State* state, lua_Debug* ar);

  // Pops a protected Lua error and preserves its diagnostic category.
  [[nodiscard]] std::expected<void, StarException> handle_error(lua_State& state, int res);
  [[nodiscard]] std::expected<void, StarException> handle_source_error(lua_State& state, int res, String const& source);
  void push_callback_error(lua_State& state, StarException const& error);
  struct DiagnosticTransport;
  using DiagnosticKey = std::variant<std::string_view, std::uintptr_t>;
  static std::optional<DiagnosticKey> diagnostic_key(lua_State& state, int index);
  std::optional<std::reference_wrapper<DiagnosticTransport const>> find_diagnostic_entry(lua_State& state, int index) const;
  std::shared_ptr<ExceptionSummary const> find_diagnostic(lua_State& state, int index) const;
  void remember_diagnostic(lua_State& state, int index, std::shared_ptr<ExceptionSummary const> summary,
      std::optional<StarException> non_recoverable_error);
  ExceptionSummary capture_diagnostic(lua_State& from, lua_State& state, int index,
      ExceptionSummaryKind kind) const;
  void begin_diagnostic_scope();
  void end_diagnostic_scope();

  // lua_pcall with a better message handler that includes a traceback.
  int pcall_with_traceback(lua_State& state, int nargs, int nresults);
  String source_diagnostic(String const& source) const noexcept;
  void append_source_diagnostics(lua_State& from, lua_State& to, ExceptionSummary const& summary) const noexcept;

  // override for lua coroutine resume with traceback
  static int coresume_with_traceback(lua_State* state);
  // propagates errors from one state to another, i.e. past thread boundaries
  // pops error off the top of the from stack and pushes onto the to stack
  static void propagate_error_with_traceback(lua_State& from, lua_State& to);

  std::size_t string_length(int handle_index);
  String string(int handle_index);
  StringView string_view(int handle_index);


  [[nodiscard]] std::expected<std::optional<LuaTable>, StarException> table_get_metatable(int handle_index);
  [[nodiscard]] std::expected<void, StarException> table_set_metatable(int handle_index, LuaTable const& table);

  template <typename Ret, typename... Args>
  [[nodiscard]] std::expected<Ret, StarException> call_function(int handle_index, Args const&... args);
  template <typename Ret> [[nodiscard]] std::expected<Ret, StarException> call_stack(lua_State& state, int nargs);
  template <typename Ret, typename... Args>
  [[nodiscard]] std::expected<std::optional<Ret>, StarException> resume_thread(int handle_index, Args const&... args);
  [[nodiscard]] std::expected<void, StarException> thread_push_function(int thread_index, int function_index);
  LuaThread::Status thread_status(int handle_index);

  template <typename T>
  [[nodiscard]] std::expected<void, StarException> register_userdata_type(LuaStack stack);

  template <typename T>
  [[nodiscard]] std::expected<bool, StarException> userdata_is_type(int handle_index);

  template <typename T>
  [[nodiscard]] std::expected<std::reference_wrapper<T>, StarException> get_userdata(int handle_index);

  [[nodiscard]] std::expected<void, StarException> set_context_require(int handle_index, LuaContext::RequireFunction require_function);

  [[nodiscard]] std::expected<int, StarException> context_eval(int handle_index, String const& lua, int nresults);
  [[nodiscard]] std::expected<void, StarException> push_context_path(int handle_index, std::string_view path);
  // Assign the validated value on top of the stack.
  [[nodiscard]] std::expected<void, StarException> set_context_path(int handle_index, std::string_view path);

  [[nodiscard]] std::expected<int, StarException> pop_handle(lua_State& state);
  void push_handle(lua_State& state, int handle_index);
  // Unref is allocation-free after the registry freelist has been initialized.
  void release_handles(lua_State& state);
  void destroy_handle(int handle_index);

  [[nodiscard]] std::expected<void, StarException> push_lua_value(lua_State& state, LuaValue const& lua_value);
  [[nodiscard]] std::expected<LuaValue, StarException> pop_lua_value(lua_State& state);
  [[nodiscard]] std::expected<std::optional<Json>, StarException> stack_to_json(lua_State& state, int index);
  // Allocates under a protected Lua call, leaving uninitialized storage on top.
  [[nodiscard]] std::expected<std::span<std::byte>, StarException> allocate_userdata(lua_State& state, std::size_t size);

  template <typename T>
  [[nodiscard]] std::expected<std::size_t, StarException> push_argument(lua_State& state, T const& arg);

  template <typename T>
  [[nodiscard]] std::expected<std::size_t, StarException> push_argument(lua_State& state, LuaVariadic<T> const& args);

  [[nodiscard]] std::expected<std::size_t, StarException> do_push_arguments(lua_State&);
  template <typename First, typename... Rest>
  [[nodiscard]] std::expected<std::size_t, StarException> do_push_arguments(lua_State& state, First const& first, Rest const&... rest);

  template <typename... Args>
  [[nodiscard]] std::expected<std::size_t, StarException> push_arguments(lua_State& state, Args const&... args);

  [[nodiscard]] std::expected<void, StarException> increment_recursion_level();
  void decrement_recursion_level();

  void update_count_hook();

  // The following fields exist to use their addresses as unique lightuserdata,
  // as is recommended by the lua docs.
  static int s_lua_instruction_limit_exception_key;
  static int s_lua_recursion_limit_exception_key;

  struct StateCloser {
    void operator()(lua_State* state) const noexcept { lua_close(state); }
  };
  std::unique_ptr<lua_State, StateCloser> m_state;
  ImageResolver m_image_resolver;
  std::unique_ptr<LuaExecutionHost> m_execution_host;
  SourceResolver m_source_resolver;
  std::stop_token m_promise_lifetime;
  // Bounded, registry-pinned error-string identities; no Lua reference owns the engine.
  struct DiagnosticTransport {
    // The registry pins string storage. A lightuserdata key is an opaque Lua
    // identity token only; it never grants access to an underlying object.
    DiagnosticKey key;
    std::shared_ptr<ExceptionSummary const> summary;
    std::optional<StarException> non_recoverable_error;
  };
  std::array<DiagnosticTransport, 64> m_diagnostic_transport;
  std::size_t m_diagnostic_count = 0;
  std::size_t m_next_diagnostic = 0;
  unsigned m_diagnostic_depth = 0;
  int m_diagnostic_strings_registry_id = LUA_NOREF;
  int m_pcall_traceback_message_handler_registry_id;
  int m_script_default_env_registry_id;
  int m_wrapped_function_metatable_registry_id;
  int m_require_function_metatable_registry_id;
  HashMap<TypeKey, int, TypeKey::Hash> m_registered_user_data_types;

  List<int> m_handle_free;

  std::uint64_t m_instruction_limit;
  bool m_profiling_enabled;
  unsigned m_instruction_measure_interval;
  std::uint64_t m_instruction_count;
  unsigned m_recursion_level;
  unsigned m_recursion_limit;
  std::string m_warning_message;
  HashMap<std::tuple<String, unsigned>, std::shared_ptr<LuaProfileEntry>> m_profile_entries;
  lua_Debug m_debug_info;
};

// Built in conversions

template <>
struct LuaConverter<bool> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, bool v) {
    lua_pushboolean(&stack.state(), v);
    return {};
  }
  [[nodiscard]] static std::expected<std::optional<bool>, StarException> to(LuaStackValue v) {
    return lua_toboolean(&v.stack().state(), v.index()) != 0;
  }
};

template <typename T>
struct LuaIntConverter {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, T v) {
    lua_pushinteger(&stack.state(), static_cast<LuaInt>(v));
    return {};
  }
  [[nodiscard]] static std::expected<std::optional<T>, StarException> to(LuaStackValue v) {
    auto from_float = [](LuaFloat value) -> std::optional<T> {
      if (!std::isfinite(value))
        return std::nullopt;
      auto truncated = std::trunc(value);
      auto upper = std::ldexp(LuaFloat(1), std::numeric_limits<T>::digits);
      auto lower = std::numeric_limits<T>::is_signed ? -upper : LuaFloat(0);
      if (truncated < lower || truncated >= upper)
        return std::nullopt;
      return static_cast<T>(truncated);
    };
    if (v.is<LuaInt>())
      return static_cast<T>(v.as<LuaInt>());
    if (v.is<LuaFloat>())
      return from_float(v.as<LuaFloat>());
    if (v.is<LuaString>()) {
      auto text = v.string_view().utf8();
      text = text.substr(0, text.find('\0'));
      if (auto n = maybe_lexical_cast<LuaInt>(text))
        return static_cast<T>(*n);
      if (auto n = maybe_lexical_cast<LuaFloat>(text))
        return from_float(*n);
    }
    return std::nullopt;
  }
};

template <>
struct LuaConverter<char> : LuaIntConverter<char> {};

template <>
struct LuaConverter<unsigned char> : LuaIntConverter<unsigned char> {};

template <>
struct LuaConverter<short> : LuaIntConverter<short> {};

template <>
struct LuaConverter<unsigned short> : LuaIntConverter<unsigned short> {};

template <>
struct LuaConverter<long> : LuaIntConverter<long> {};

template <>
struct LuaConverter<unsigned long> : LuaIntConverter<unsigned long> {};

template <>
struct LuaConverter<int> : LuaIntConverter<int> {};

template <>
struct LuaConverter<unsigned int> : LuaIntConverter<unsigned int> {};

template <>
struct LuaConverter<long long> : LuaIntConverter<long long> {};

template <>
struct LuaConverter<unsigned long long> : LuaIntConverter<unsigned long long> {};

template <typename T>
struct LuaFloatConverter {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, T v) {
    lua_pushnumber(&stack.state(), static_cast<LuaFloat>(v));
    return {};
  }
  [[nodiscard]] static std::expected<std::optional<T>, StarException> to(LuaStackValue v) {
    if (v.is<LuaFloat>())
      return static_cast<T>(v.as<LuaFloat>());
    if (v.is<LuaInt>())
      return static_cast<T>(v.as<LuaInt>());
    if (v.is<LuaString>()) {
      auto text = v.string_view().utf8();
      text = text.substr(0, text.find('\0'));
      if (auto n = maybe_lexical_cast<LuaFloat>(text))
        return static_cast<T>(*n);
      if (auto n = maybe_lexical_cast<LuaInt>(text))
        return static_cast<T>(*n);
    }
    return std::nullopt;
  }
};

template <>
struct LuaConverter<float> : LuaFloatConverter<float> {};

template <>
struct LuaConverter<double> : LuaFloatConverter<double> {};

template <>
struct LuaConverter<String> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, String const& v) {
    return stack.push_string(v.utf8());
  }
  [[nodiscard]] static std::expected<std::optional<String>, StarException> to(LuaStackValue v) {
    if (v.is<LuaString>())
      return v.to_string();
    if (v.is<LuaInt>())
      return String(Star::to_string(v.as<LuaInt>()));
    if (v.is<LuaFloat>())
      return String(Star::to_string(v.as<LuaFloat>()));
    return std::nullopt;
  }
};

template <>
struct LuaConverter<std::string> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, std::string const& v) {
    return stack.push_string(v);
  }
  [[nodiscard]] static std::expected<std::optional<std::string>, StarException> to(LuaStackValue v) {
    if (v.is<LuaString>())
      return std::string(v.string_view().utf8());
    auto converted = v.maybe_to<String>();
    if (!converted)
      return std::unexpected(std::move(converted.error()));
    if (!*converted)
      return std::nullopt;
    return std::move(**converted).take_utf8();
  }
};

template <>
struct LuaConverter<std::string_view> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, std::string_view v) {
    return stack.push_string(v);
  }
};

template <std::size_t s>
struct LuaConverter<char[s]> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, char const (&v)[s]) {
    auto bytes = std::string_view(v, s);
    return stack.push_string(bytes.substr(0, bytes.find('\0')));
  }
};

template <>
struct LuaConverter<Directives> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, Directives const& v) {
    auto string = v.maybe_string();
    return stack.push_string(string ? string->get().utf8() : std::string_view());
  }
};

template <typename T>
struct LuaValueConverter {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, T const& v) {
    return stack.push(LuaValue(v));
  }
  [[nodiscard]] static std::expected<std::optional<T>, StarException> to(LuaStackValue v) {
    if (!v.is<T>())
      return std::nullopt;
    auto owned = v.own();
    if (!owned)
      return std::unexpected(std::move(owned.error()));
    return std::move(std::get<T>(*owned));
  }
};

template <> struct LuaConverter<LuaString> : LuaValueConverter<LuaString> {
  [[nodiscard]] static std::expected<std::optional<LuaString>, StarException> to(LuaStackValue v) {
    if (v.is<LuaString>())
      return LuaValueConverter<LuaString>::to(v);
    if (v.is<LuaInt>())
      return v.stack().engine().create_string(Star::to_string(v.as<LuaInt>()));
    if (v.is<LuaFloat>())
      return v.stack().engine().create_string(Star::to_string(v.as<LuaFloat>()));
    return std::nullopt;
  }
};

template <>
struct LuaConverter<LuaTable> : LuaValueConverter<LuaTable> {};
template <>
struct LuaConverter<LuaFunction> : LuaValueConverter<LuaFunction> {};

template <>
struct LuaConverter<LuaDetail::LuaWrappedFunction> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, LuaDetail::LuaWrappedFunction function);
};
template <>
struct LuaConverter<LuaThread> : LuaValueConverter<LuaThread> {};
template <>
struct LuaConverter<LuaUserData> : LuaValueConverter<LuaUserData> {};

template <>
struct LuaConverter<LuaValue> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, LuaValue const& v) {
    return stack.engine().push_lua_value(stack.state(), v);
  }
  [[nodiscard]] static std::expected<LuaValue, StarException> to(LuaStackValue v) { return v.own(); }
};

template <> struct LuaConverter<LuaStackValue> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, LuaStackValue v) {
    if (&stack.engine() != &v.stack().engine())
      return std::unexpected(LuaException("Lua stack values cannot be shared between engines"));
    if (&stack.state() != &v.stack().state() && !lua_checkstack(&v.stack().state(), 1))
      return std::unexpected(LuaException("Exhausted the Lua stack while copying a borrowed value"));
    lua_pushvalue(&v.stack().state(), v.index());
    if (&stack.state() != &v.stack().state())
      lua_xmove(&v.stack().state(), &stack.state(), 1);
    return {};
  }
};

template <typename T>
struct LuaConverter<std::optional<T>> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, std::optional<T> const& v)
    requires LuaDetail::lua_pushable<T const&>
  {
    return v ? stack.push(*v) : stack.push(lua_nil);
  }
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, std::optional<T>&& v)
    requires (std::is_convertible_v<T&&, T> && LuaDetail::lua_pushable<T>)
  {
    if (!v)
      return stack.push(lua_nil);
    T value = std::move(*v);
    v.reset();
    return stack.push(std::move(value));
  }
  [[nodiscard]] static std::expected<std::optional<std::optional<T>>, StarException> to(LuaStackValue v)
    requires (std::is_constructible_v<T, T&&> && requires(LuaStackValue value) { LuaConverter<T>::to(value); })
  {
    if (v.is<LuaNilType>())
      return std::optional<std::optional<T>>(std::in_place);
    auto converted = v.maybe_to<T>();
    if (!converted)
      return std::unexpected(std::move(converted.error()));
    if (!*converted)
      return std::nullopt;
    return std::optional<std::optional<T>>(std::in_place, std::move(*converted));
  }
};

template <typename T>
struct LuaMapConverter {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, T const& v) { return stack.push_map(v); }
  [[nodiscard]] static std::expected<std::optional<T>, StarException> to(LuaStackValue v) {
    if (!v.is<LuaTable>())
      return std::nullopt;
    T result;
    bool mismatch = false;
    auto iterated = v.iterate([&](LuaStackValue key, LuaStackValue value) -> std::expected<bool, StarException> {
      auto cont_key = key.maybe_to<typename T::key_type>();
      if (!cont_key)
        return std::unexpected(std::move(cont_key.error()));
      auto cont_value = value.maybe_to<typename T::mapped_type>();
      if (!cont_value)
        return std::unexpected(std::move(cont_value.error()));
      if (!*cont_key || !*cont_value) {
        mismatch = true;
        return false;
      }
      result.set(std::move(**cont_key), std::move(**cont_value));
      return true;
    });
    if (!iterated)
      return std::unexpected(std::move(iterated.error()));
    if (mismatch)
      return std::nullopt;
    return std::optional<T>(std::in_place, std::move(result));
  }
};

template <typename T>
struct LuaContainerConverter {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, T const& v) {
    return stack.push_array(v);
  }
  [[nodiscard]] static std::expected<std::optional<T>, StarException> to(LuaStackValue v) {
    if (!v.is<LuaTable>())
      return std::nullopt;
    T result;
    bool mismatch = false;
    auto iterated = v.iterate([&](LuaStackValue key, LuaStackValue value) -> std::expected<bool, StarException> {
      if (!key.is<LuaInt>()) {
        mismatch = true;
        return false;
      }
      auto converted = value.maybe_to<typename T::value_type>();
      if (!converted)
        return std::unexpected(std::move(converted.error()));
      if (!*converted) {
        mismatch = true;
        return false;
      }
      if constexpr (requires { result.insert(result.end(), std::move(**converted)).has_value(); }) {
        auto inserted = result.insert(result.end(), std::move(**converted));
        if (!inserted)
          return std::unexpected(std::move(inserted.error()));
      } else {
        result.insert(result.end(), std::move(**converted));
      }
      return true;
    });
    if (!iterated)
      return std::unexpected(std::move(iterated.error()));
    if (mismatch)
      return std::nullopt;
    return std::optional<T>(std::in_place, std::move(result));
  }
};

template <typename T, typename Allocator>
struct LuaConverter<List<T, Allocator>> : LuaContainerConverter<List<T, Allocator>> {};

template <typename T, std::size_t max_size>
struct LuaConverter<StaticList<T, max_size>> : LuaContainerConverter<StaticList<T, max_size>> {};

template <typename T, std::size_t max_stack_size>
struct LuaConverter<SmallList<T, max_stack_size>> : LuaContainerConverter<SmallList<T, max_stack_size>> {};

template <>
struct LuaConverter<StringList> : LuaContainerConverter<StringList> {};

template <set_container Container>
struct LuaConverter<Container> : LuaContainerConverter<Container> {};

template <typename Key, typename Value, typename Compare, typename Allocator>
struct LuaConverter<Map<Key, Value, Compare, Allocator>> : LuaMapConverter<Map<Key, Value, Compare, Allocator>> {};

template <typename Key, typename Value, typename Hash, typename Equals, typename Allocator>
struct LuaConverter<HashMap<Key, Value, Hash, Equals, Allocator>> : LuaMapConverter<HashMap<Key, Value, Hash, Equals, Allocator>> {};

template <>
struct LuaConverter<Json> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, Json const& v);
  [[nodiscard]] static std::expected<std::optional<Json>, StarException> to(LuaStackValue v);
};

template <>
struct LuaConverter<JsonObject> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, JsonObject const& v);
  [[nodiscard]] static std::expected<std::optional<JsonObject>, StarException> to(LuaStackValue v);
};

template <>
struct LuaConverter<JsonArray> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, JsonArray const& v);
  [[nodiscard]] static std::expected<std::optional<JsonArray>, StarException> to(LuaStackValue v);
};


namespace LuaDetail {
  inline LuaRegistryReference::LuaRegistryReference(LuaEnginePtr engine, int handle_index)
    : engine(std::move(engine)), handle_index(handle_index) {}

  inline LuaRegistryReference::~LuaRegistryReference() {
    if (engine)
      engine->destroy_handle(handle_index);
  }

  inline LuaHandle::LuaHandle(LuaEnginePtr engine, int handle_index)
    : reference(std::make_shared<LuaRegistryReference>(std::move(engine), handle_index)) {}

  // Continuations retain converted handles and stop at the first error.
  template <typename... Types>
  struct ConvertedPack;
  template <>
  struct ConvertedPack<> {
    template <typename Getter, typename Function>
    [[nodiscard]] static auto apply(Getter&, std::size_t, Function&& function) {
      return std::invoke(std::forward<Function>(function));
    }
  };
  template <typename First, typename... Rest>
  struct ConvertedPack<First, Rest...> {
    template <typename Getter, typename Function>
    [[nodiscard]] static std::invoke_result_t<Function, First, Rest...> apply(Getter& getter, std::size_t index, Function&& function) {
      auto first = getter.template operator()<First>(index);
      if (!first)
        return std::unexpected(std::move(first.error()));
      return ConvertedPack<Rest...>::apply(getter, index + 1,
          [&](auto&&... rest) -> std::invoke_result_t<Function, First, Rest...> {
            return std::invoke(std::forward<Function>(function), std::move(*first), std::forward<decltype(rest)>(rest)...);
          });
    }
  };

  template <typename T> struct LuaReturn {
    static constexpr int count = 1;
    [[nodiscard]] static std::expected<T, StarException> read(LuaStack stack, int first, int) requires lua_readable<T> {
      return stack.read<T>(first);
    }
    template <lua_pushable Value>
      requires std::same_as<std::remove_cvref_t<Value>, T>
    [[nodiscard]] static std::expected<int, StarException> push(LuaStack stack, Value&& value) {
      auto pushed = stack.push(std::forward<Value>(value));
      if (!pushed)
        return std::unexpected(std::move(pushed.error()));
      return 1;
    }
  };
  template <> struct LuaReturn<void> {
    static constexpr int count = 0;
    [[nodiscard]] static std::expected<void, StarException> read(LuaStack, int, int) { return {}; }
  };
  template <typename T> struct LuaReturn<LuaVariadic<T>> {
    static constexpr int count = LUA_MULTRET;
    [[nodiscard]] static std::expected<LuaVariadic<T>, StarException> read(LuaStack stack, int first, int count)
      requires lua_readable<T> {
      LuaVariadic<T> result;
      result.reserve(count);
      for (int i = 0; i < count; ++i) {
        auto value = stack.read<T>(first + i);
        if (!value)
          return std::unexpected(std::move(value.error()));
        result.push_back(std::move(*value));
      }
      return result;
    }
    // Forward actual elements, but materialize the value of a proxy (vector<bool>).
    template <typename Values>
    using PushedElement = std::conditional_t<std::is_reference_v<std::ranges::range_reference_t<Values>>,
      decltype(std::forward_like<Values>(std::declval<T&>())), T>;
    template <typename Values>
      requires std::same_as<std::remove_cvref_t<Values>, LuaVariadic<T>> && lua_pushable<PushedElement<Values>>
    [[nodiscard]] static std::expected<int, StarException> push(LuaStack stack, Values&& values) {
      if (values.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        return std::unexpected(LuaException("Too many Lua return values"));
      for (auto&& value : values) {
        auto pushed = stack.push(static_cast<PushedElement<Values>>(value));
        if (!pushed)
          return std::unexpected(std::move(pushed.error()));
      }
      return static_cast<int>(values.size());
    }
  };
  template <typename... Types> struct LuaReturn<LuaTupleReturn<Types...>> {
    static constexpr int count = sizeof...(Types);
    [[nodiscard]] static std::expected<LuaTupleReturn<Types...>, StarException> read(LuaStack stack, int first, int count)
      requires (lua_readable<Types> && ...) {
      auto getter = [&]<typename T>(std::size_t index) {
        return stack.read<T>(index < static_cast<std::size_t>(count) ? first + static_cast<int>(index)
                                                                     : stack.top() + 1);
      };
      return ConvertedPack<Types...>::apply(getter, 0,
          [](Types... values) -> std::expected<LuaTupleReturn<Types...>, StarException> {
            return LuaTupleReturn<Types...>(std::move(values)...);
          });
    }
    template <typename Values>
      requires std::same_as<std::remove_cvref_t<Values>, LuaTupleReturn<Types...>>
        && ([]<std::size_t... indices>(std::index_sequence<indices...>) {
          return (lua_pushable<decltype(std::get<indices>(std::declval<Values>()))> && ...);
        }(std::index_sequence_for<Types...>{}))
    [[nodiscard]] static std::expected<int, StarException> push(LuaStack stack, Values&& values) {
      std::expected<void, StarException> status;
      auto append = [&]<typename Value>(Value&& value) {
        if (status)
          status = stack.push(std::forward<Value>(value));
      };
      [&]<std::size_t... indices>(std::index_sequence<indices...>) {
        (append(std::get<indices>(std::forward<Values>(values))), ...);
      }(std::index_sequence_for<Types...>{});
      if (!status)
        return std::unexpected(std::move(status.error()));
      return count;
    }
  };

  // A matching declared type is not a conversion: retain the source's cv/ref category.
  template <typename Declared, typename Value>
  using WrappedValue = std::conditional_t<
    std::same_as<std::remove_cvref_t<Declared>, std::remove_cvref_t<Value>>, Value&&, Declared>;

  template <typename Declared, typename Value>
    requires requires(LuaStack stack, Value&& value) {
      LuaReturn<std::remove_cvref_t<Declared>>::push(
        stack, static_cast<WrappedValue<Declared, Value>>(std::forward<Value>(value)));
    }
  [[nodiscard]] std::expected<int, StarException> to_wrapped_return(LuaStack stack, Value&& value) {
    return LuaReturn<std::remove_cvref_t<Declared>>::push(
      stack, static_cast<WrappedValue<Declared, Value>>(std::forward<Value>(value)));
  }
  template <typename Return, typename Function, typename... Args>
    requires std::invocable<Function, Args...>
  [[nodiscard]] std::expected<int, StarException> invoke_wrapped(LuaStack stack, Function&& function, Args&&... args) {
    using DeclaredReturn = typename AlgorithmResultInfo<std::remove_cvref_t<Return>>::value_type;
    using ActualReturn = std::remove_cvref_t<std::invoke_result_t<Function, Args...>>;
    using ActualResultInfo = AlgorithmResultInfo<ActualReturn>;
    if constexpr (std::is_void_v<ActualReturn>) {
      static_assert(std::is_void_v<DeclaredReturn>);
      std::invoke(std::forward<Function>(function), std::forward<Args>(args)...);
      return LuaReturn<LuaValue>::push(stack, lua_nil);
    } else {
      auto&& result = std::invoke(std::forward<Function>(function), std::forward<Args>(args)...);
      if constexpr (ActualResultInfo::is_fallible) {
        if (!result)
          return std::unexpected(std::forward<decltype(result)>(result).error());
        if constexpr (std::is_void_v<DeclaredReturn>)
          return LuaReturn<LuaValue>::push(stack, lua_nil);
        else
          return to_wrapped_return<DeclaredReturn>(stack, *std::forward<decltype(result)>(result));
      } else if constexpr (std::is_void_v<DeclaredReturn>) {
        return LuaReturn<LuaValue>::push(stack, lua_nil);
      } else {
        return to_wrapped_return<DeclaredReturn>(stack, std::forward<decltype(result)>(result));
      }
    }
  }

  template <typename T>
  struct ArgGet {
    [[nodiscard]] static std::expected<T, StarException> get(LuaStack stack, std::size_t index) {
      return stack.read<T>(static_cast<int>(index) + 1);
    }
  };
  template <typename T>
  struct ArgGet<LuaVariadic<T>> {
    [[nodiscard]] static std::expected<LuaVariadic<T>, StarException> get(LuaStack stack, std::size_t index) {
      int first = static_cast<int>(index) + 1;
      return LuaReturn<LuaVariadic<T>>::read(stack, first, std::max(0, stack.top() - first + 1));
    }
  };
  template <typename Return, typename... Args>
  struct FunctionWrapper {
    template <typename Function>
    static LuaWrappedFunction wrap(Function function) {
      return [function = std::move(function)](LuaStack stack) mutable -> std::expected<int, StarException> {
        auto getter = [&]<typename T>(std::size_t index) { return ArgGet<T>::get(stack, index); };
        return ConvertedPack<Args...>::apply(
          getter, 0, [&](Args&&... args) { return invoke_wrapped<Return>(stack, function, std::move(args)...); });
      };
    }
  };
  template <typename Return, typename... Args>
  struct FunctionWrapper<Return, LuaEngine, Args...> {
    template <typename Function>
    static LuaWrappedFunction wrap(Function function) {
      return [function = std::move(function)](LuaStack stack) mutable -> std::expected<int, StarException> {
        auto getter = [&]<typename T>(std::size_t index) { return ArgGet<T>::get(stack, index); };
        return ConvertedPack<Args...>::apply(getter, 0, [&](Args&&... args) {
          return invoke_wrapped<Return>(stack, function, stack.engine(), std::move(args)...);
        });
      };
    }
  };
  template <typename Return, typename... Args, typename Function>
  LuaWrappedFunction wrap_function_with_signature(Function&& function) {
    return FunctionWrapper<Return, std::decay_t<Args>...>::wrap(std::forward<Function>(function));
  }
  template <typename Return, typename Function, typename... Args>
  LuaWrappedFunction wrap_function_args(Function&& function, std::type_identity<std::tuple<Args...>>) {
    return wrap_function_with_signature<Return, Args...>(std::forward<Function>(function));
  }
  template <signature_deducible Function>
  LuaWrappedFunction wrap_function(Function&& function) {
    return wrap_function_args<typename FunctionTraits<Function>::result_type>(
      std::forward<Function>(function), std::type_identity<typename FunctionTraits<Function>::argument_types>{});
  }

  template <typename Return, typename T, typename... Args>
  struct MethodWrapper {
    template <typename Function>
    static LuaWrappedFunction wrap(Function function) {
      return [function = std::move(function)](LuaStack stack) mutable -> std::expected<int, StarException> {
        if (stack.top() == 0)
          return std::unexpected(LuaException("No object argument passed to wrapped method"));
        auto object = stack.value(1).userdata<T>();
        if (!object)
          return std::unexpected(std::move(object.error()));
        auto getter = [&]<typename A>(std::size_t index) { return ArgGet<A>::get(stack, index + 1); };
        return ConvertedPack<Args...>::apply(getter, 0, [&](Args&&... args) {
          return invoke_wrapped<Return>(stack, function, object->get(), std::move(args)...);
        });
      };
    }
  };
  template <typename Return, typename T, typename... Args>
  struct MethodWrapper<Return, T, LuaEngine, Args...> {
    template <typename Function>
    static LuaWrappedFunction wrap(Function function) {
      return [function = std::move(function)](LuaStack stack) mutable -> std::expected<int, StarException> {
        if (stack.top() == 0)
          return std::unexpected(LuaException("No object argument passed to wrapped method"));
        auto object = stack.value(1).userdata<T>();
        if (!object)
          return std::unexpected(std::move(object.error()));
        auto getter = [&]<typename A>(std::size_t index) { return ArgGet<A>::get(stack, index + 1); };
        return ConvertedPack<Args...>::apply(getter, 0, [&](Args&&... args) {
          return invoke_wrapped<Return>(stack, function, object->get(), stack.engine(), std::move(args)...);
        });
      };
    }
  };
  template <typename Return, typename... Args, typename Function>
  LuaWrappedFunction wrap_method_with_signature(Function&& function) {
    return MethodWrapper<Return, std::decay_t<Args>...>::wrap(std::forward<Function>(function));
  }
  template <typename Return, typename Function, typename... Args>
  LuaWrappedFunction wrap_method_args(Function&& function, std::type_identity<std::tuple<Args...>>) {
    return wrap_method_with_signature<Return, Args...>(std::forward<Function>(function));
  }
  template <signature_deducible Function>
  LuaWrappedFunction wrap_method(Function&& function) {
    return wrap_method_args<typename FunctionTraits<Function>::result_type>(
      std::forward<Function>(function), std::type_identity<typename FunctionTraits<Function>::argument_types>{});
  }

  template <typename Function, typename... Args>
  [[nodiscard]] std::expected<bool, StarException> invoke_iterator(Function& function, Args&&... args) {
    using Return = std::invoke_result_t<Function&, Args...>;
    if constexpr (std::is_void_v<Return>) {
      std::invoke(function, std::forward<Args>(args)...);
      return true;
    } else if constexpr (AlgorithmResultInfo<Return>::is_fallible) {
      auto result = std::invoke(function, std::forward<Args>(args)...);
      if (!result)
        return std::unexpected(std::move(result.error()));
      if constexpr (std::is_void_v<typename AlgorithmResultInfo<Return>::value_type>)
        return true;
      else
        return *result;
    } else {
      return std::invoke(function, std::forward<Args>(args)...);
    }
  }
  template <typename Ret, typename... Args>
  struct TableIteratorWrapper;
  template <typename Ret, typename Key, typename Value>
  struct TableIteratorWrapper<Ret, Key, Value> {
    template <typename Function> static auto wrap(LuaEngine&, Function&& function) {
      return [function = std::forward<Function>(function)](
               LuaStackValue key, LuaStackValue value) mutable -> std::expected<bool, StarException> {
        auto converted_key = key.template to<Key>();
        if (!converted_key)
          return std::unexpected(std::move(converted_key.error()));
        auto converted_value = value.template to<Value>();
        if (!converted_value)
          return std::unexpected(std::move(converted_value.error()));
        return invoke_iterator(function, std::move(*converted_key), std::move(*converted_value));
      };
    }
  };
  template <typename Ret, typename Key, typename Value>
  struct TableIteratorWrapper<Ret, LuaEngine, Key, Value> {
    template <typename Function> static auto wrap(LuaEngine& engine, Function&& function) {
      return [&engine, function = std::forward<Function>(function)](
               LuaStackValue key, LuaStackValue value) mutable -> std::expected<bool, StarException> {
        auto converted_key = key.template to<Key>();
        if (!converted_key)
          return std::unexpected(std::move(converted_key.error()));
        auto converted_value = value.template to<Value>();
        if (!converted_value)
          return std::unexpected(std::move(converted_value.error()));
        return invoke_iterator(function, engine, std::move(*converted_key), std::move(*converted_value));
      };
    }
  };
  template <typename Return, typename... Args, typename Function>
  auto wrap_table_iterator_with_signature(LuaEngine& engine, Function&& function) {
    return TableIteratorWrapper<Return, std::decay_t<Args>...>::wrap(engine, std::forward<Function>(function));
  }
  template <typename Return, typename Function, typename... Args>
  auto wrap_table_iterator_args(LuaEngine& engine, Function&& function, std::type_identity<std::tuple<Args...>>) {
    return wrap_table_iterator_with_signature<Return, Args...>(engine, std::forward<Function>(function));
  }
  template <signature_deducible Function> auto wrap_table_iterator(LuaEngine& engine, Function&& function) {
    return wrap_table_iterator_args<typename FunctionTraits<Function>::result_type>(
      engine, std::forward<Function>(function), std::type_identity<typename FunctionTraits<Function>::argument_types>{});
  }

  // Like lua_setfield but raw; used inside protected Lua calls.
  void raw_set_field(lua_State& state, int index, std::string_view key);

  // Shallow copies a lua table at the given index into the table at the target
  // index.
  void shallow_copy(lua_State& state, int source_index, int target_index);


  // Creates a custom lua table from a JsonArray or JsonObject that has
  // slightly different behavior than a standard lua table.  The table
  // remembers nil entries, as well as whether it was initially constructed
  // from a JsonArray or JsonObject as a hint on how to convert it back into a
  // Json.  The custom containers are meant to act nearly identical to standard
  // lua tables, so iterating over the table with pairs or ipairs works exactly
  // like a standard lua table, so will skip over nil entries and in the case
  // of ipairs, stop at the first nil entry.
  [[nodiscard]] std::expected<LuaTable, StarException> json_container_to_table(LuaEngine& engine, Json const& container);


  // Special lua functions to operate on our custom jarray / jobject container
  // types.  Should always do some "sensible" action if given a regular lua
  // table instead of a custom json container one.

  // Create a JsonList container table
  Json jarray_create();
  // Create a JsonMap container table
  Json jobject_create();

  // Adds the Json array metatable to a Lua table or creates one.
  [[nodiscard]] std::expected<LuaTable, StarException> jarray(LuaEngine& engine, std::optional<LuaTable> table);
  // Adds the Json object metatable to a Lua table or creates one.
  [[nodiscard]] std::expected<LuaTable, StarException> jobject(LuaEngine& engine, std::optional<LuaTable> table);

  // *Really* remove an entry from a JsonList or JsonMap container table,
  // including removing it from the __nils table.  If the given table is not a
  // special container table, is equivalent to setting the key entry to nil.
  [[nodiscard]] std::expected<void, StarException> jcont_remove(LuaTable const& t, LuaValue const& key);

  // Returns the element count of the lua table argument, or, in the case of a
  // special JsonList container table, returns the "true" element count
  // including any nil entries.
  [[nodiscard]] std::expected<std::size_t, StarException> jcont_size(LuaTable const& t);

  // Resize the given lua table by removing any indexed entries greater than the
  // target size, and in the case of a special JsonList container table, pads
  // to the end of the new size with nil entries.
  [[nodiscard]] std::expected<void, StarException> jcont_resize(LuaTable const& t, std::size_t size);

  // Coerces a values (strings, floats, ints) into an integer, but fails if the
  // number looks fractional (does not parse as int, float is not an exact
  // integer)
  std::optional<LuaInt> as_integer(LuaValue const& v);
}

template <typename Container> LuaVariadic<typename std::decay_t<Container>::value_type> lua_unpack(Container&& c) {
  LuaVariadic<typename std::decay_t<Container>::value_type> ret;
  ret.append_range(std::forward<Container>(c));
  return ret;
}

template <typename... Types>
LuaTupleReturn<Types...>::LuaTupleReturn(Types const&... args)
  : Base(args...) {}

template <typename... Types>
template <typename... UTypes>
  requires std::constructible_from<std::tuple<Types...>, UTypes&&...>
LuaTupleReturn<Types...>::LuaTupleReturn(UTypes&&... args)
  : Base(std::forward<UTypes>(args)...) {}

template <typename... Types>
template <typename... UTypes>
LuaTupleReturn<Types...>::LuaTupleReturn(UTypes const&... args)
  : Base(args...) {}

template <typename... Types>
LuaTupleReturn<Types...>::LuaTupleReturn(LuaTupleReturn const& rhs)
  : Base(rhs) {}

template <typename... Types>
LuaTupleReturn<Types...>::LuaTupleReturn(LuaTupleReturn&& rhs)
  : Base(std::move(rhs)) {}

template <typename... Types>
template <typename... UTypes>
LuaTupleReturn<Types...>::LuaTupleReturn(LuaTupleReturn<UTypes...> const& rhs)
  : Base(rhs) {}

template <typename... Types>
template <typename... UTypes>
LuaTupleReturn<Types...>::LuaTupleReturn(LuaTupleReturn<UTypes...>&& rhs)
  : Base(std::move(rhs)) {}

template <typename... Types>
LuaTupleReturn<Types...>& LuaTupleReturn<Types...>::operator=(LuaTupleReturn const& rhs) {
  Base::operator=(rhs);
  return *this;
}

template <typename... Types>
LuaTupleReturn<Types...>& LuaTupleReturn<Types...>::operator=(LuaTupleReturn&& rhs) {
  Base::operator=(std::move(rhs));
  return *this;
}

template <typename... Types>
template <typename... UTypes>
LuaTupleReturn<Types...>& LuaTupleReturn<Types...>::operator=(LuaTupleReturn<UTypes...> const& rhs) {
  Base::operator=((std::tuple<UTypes...> const&)rhs);
  return *this;
}

template <typename... Types>
template <typename... UTypes>
LuaTupleReturn<Types...>& LuaTupleReturn<Types...>::operator=(LuaTupleReturn<UTypes...>&& rhs) {
  Base::operator=((std::tuple<UTypes...> && )std::move(rhs));
  return *this;
}

template <typename... Types>
LuaTupleReturn<Types&...> lua_tie(Types&... args) {
  return LuaTupleReturn<Types&...>(args...);
}

template <typename... Types> LuaTupleReturn<std::decay_t<Types>...> lua_tuple_return(Types&&... args) {
  return LuaTupleReturn<std::decay_t<Types>...>(std::forward<Types>(args)...);
}

namespace LuaDetail {
  struct LuaStackRestore {
    lua_State& state;
    int top;

    explicit LuaStackRestore(lua_State& state) : state(state), top(lua_gettop(&state)) {}
    LuaStackRestore(LuaStackRestore const&) = delete;
    LuaStackRestore& operator=(LuaStackRestore const&) = delete;
    ~LuaStackRestore() { lua_settop(&state, top); }
  };

  template <typename T>
  inline constexpr bool lua_value_alternative = []<typename... Types>(std::type_identity<std::variant<Types...>>) {
    return (std::is_same_v<std::remove_cvref_t<T>, Types> || ...);
  }(std::type_identity<LuaValue>{});

  template <typename T, typename Result>
  [[nodiscard]] std::expected<std::optional<T>, StarException> normalize_maybe_to(Result&& result) {
    using R = std::remove_cvref_t<Result>;
    if constexpr (requires { typename R::error_type; }) {
      if (!result)
        return std::unexpected(std::move(result.error()));
      if constexpr (std::is_same_v<typename R::value_type, std::optional<T>>)
        return std::move(*result);
      else
        return std::optional<T>(std::in_place, std::move(*result));
    } else if constexpr (std::is_same_v<R, std::optional<T>>) {
      return std::forward<Result>(result);
    } else {
      return std::optional<T>(std::in_place, std::forward<Result>(result));
    }
  }

  }// namespace LuaDetail

  inline LuaStackValue LuaStack::value(int index) const { return LuaStackValue(*this, index); }

  template <typename T> [[nodiscard]] std::expected<void, StarException> LuaStack::push(T&& value) const {
    LuaDetail::LuaStackRestore restore(m_state);
    if (!lua_checkstack(&m_state, 4))
      return std::unexpected(LuaException("Exhausted the Lua stack while pushing a value"));
    std::expected<void, StarException> pushed = LuaConverter<std::remove_cvref_t<T>>::from(*this, std::forward<T>(value));
    if (pushed)
      ++restore.top;
    return pushed;
  }

  template <typename T> [[nodiscard]] std::expected<T, StarException> LuaStack::read(int index) const {
    return value(index).to<T>();
  }
  template <typename T>
  [[nodiscard]] std::expected<std::optional<T>, StarException> LuaStack::maybe_read(int index) const {
    return value(index).maybe_to<T>();
  }

  template <typename T> bool LuaStackValue::is() const {
    auto& state = m_stack.state();
    if constexpr (std::is_same_v<T, LuaNilType>)
      return lua_isnoneornil(&state, m_index);
    else if constexpr (std::is_same_v<T, LuaBoolean>)
      return lua_type(&state, m_index) == LUA_TBOOLEAN;
    else if constexpr (std::is_same_v<T, LuaInt>)
      return lua_isinteger(&state, m_index);
    else if constexpr (std::is_same_v<T, LuaFloat>)
      return lua_type(&state, m_index) == LUA_TNUMBER && !lua_isinteger(&state, m_index);
    else if constexpr (std::is_same_v<T, LuaString>)
      return lua_type(&state, m_index) == LUA_TSTRING;
    else if constexpr (std::is_same_v<T, LuaTable>)
      return lua_type(&state, m_index) == LUA_TTABLE;
    else if constexpr (std::is_same_v<T, LuaFunction>)
      return lua_type(&state, m_index) == LUA_TFUNCTION;
    else if constexpr (std::is_same_v<T, LuaThread>)
      return lua_type(&state, m_index) == LUA_TTHREAD;
    else {
      static_assert(std::is_same_v<T, LuaUserData>);
      return lua_type(&state, m_index) == LUA_TUSERDATA;
    }
  }
  template <typename T> T LuaStackValue::as() const {
    if constexpr (std::is_same_v<T, LuaBoolean>)
      return lua_toboolean(&m_stack.state(), m_index) != 0;
    else if constexpr (std::is_same_v<T, LuaInt>)
      return lua_tointeger(&m_stack.state(), m_index);
    else {
      static_assert(std::is_same_v<T, LuaFloat>);
      return lua_tonumber(&m_stack.state(), m_index);
    }
  }
  template <typename T> [[nodiscard]] std::expected<std::optional<T>, StarException> LuaStackValue::maybe_to() const {
    if (auto valid = validate(); !valid)
      return std::unexpected(std::move(valid.error()));
    return LuaDetail::normalize_maybe_to<T>(LuaConverter<T>::to(*this));
  }
  template <typename T> [[nodiscard]] std::expected<T, StarException> LuaStackValue::to() const {
    auto converted = maybe_to<T>();
    if (!converted)
      return std::unexpected(std::move(converted.error()));
    if (!*converted)
      return std::unexpected(LuaEngine::conversion_error<T>());
    return std::move(**converted);
  }

  template <typename K>
  [[nodiscard]] std::expected<void, StarException> LuaStackValue::push_field(K const& key, bool raw) const {
    auto& state = m_stack.state();
    if (!lua_checkstack(&state, 5))
      return std::unexpected(LuaException("Exhausted the Lua stack while reading a table"));
    lua_pushvalue(&state, m_index);
    if (auto pushed = m_stack.push(key); !pushed)
      return std::unexpected(std::move(pushed.error()));
    return m_stack.call(raw ? +[](lua_State* s) { lua_rawget(s, 1); return 1; }
                         : +[](lua_State* s) { lua_gettable(s, 1); return 1; }, 2, 1);
  }
  template <typename T, typename K>
  [[nodiscard]] std::expected<T, StarException> LuaStackValue::get(K const& key) const {
    LuaDetail::LuaStackRestore restore(m_stack.state());
    if (auto pushed = push_field(key, false); !pushed)
      return std::unexpected(std::move(pushed.error()));
    return m_stack.read<T>(-1);
  }
  template <typename T, typename K>
  [[nodiscard]] std::expected<std::optional<T>, StarException> LuaStackValue::maybe_get(K const& key) const {
    LuaDetail::LuaStackRestore restore(m_stack.state());
    if (auto pushed = push_field(key, false); !pushed)
      return std::unexpected(std::move(pushed.error()));
    return m_stack.maybe_read<T>(-1);
  }
  template <typename K> [[nodiscard]] std::expected<bool, StarException> LuaStackValue::contains(K const& key) const {
    LuaDetail::LuaStackRestore restore(m_stack.state());
    if (auto pushed = push_field(key, false); !pushed)
      return std::unexpected(std::move(pushed.error()));
    if (auto valid = m_stack.value(-1).validate(); !valid)
      return std::unexpected(std::move(valid.error()));
    return !m_stack.value(-1).is<LuaNilType>();
  }
  template <typename T, typename K>
  [[nodiscard]] std::expected<T, StarException> LuaStackValue::raw_get(K const& key) const {
    LuaDetail::LuaStackRestore restore(m_stack.state());
    if (auto pushed = push_field(key, true); !pushed)
      return std::unexpected(std::move(pushed.error()));
    return m_stack.read<T>(-1);
  }
  template <typename K, typename V>
  [[nodiscard]] std::expected<void, StarException> LuaStackValue::set_field(K const& key, V&& value, bool raw) const {
    auto& state = m_stack.state();
    LuaDetail::LuaStackRestore restore(state);
    if (!lua_checkstack(&state, 6))
      return std::unexpected(LuaException("Exhausted the Lua stack while writing a table"));
    lua_pushvalue(&state, m_index);
    if (auto pushed = m_stack.push(key); !pushed)
      return std::unexpected(std::move(pushed.error()));
    if (auto pushed = m_stack.push(std::forward<V>(value)); !pushed)
      return std::unexpected(std::move(pushed.error()));
    return m_stack.call(raw ? +[](lua_State* s) { lua_rawset(s, 1); return 0; }
                         : +[](lua_State* s) { lua_settable(s, 1); return 0; }, 3, 0);
  }
  template <typename K, typename V>
  [[nodiscard]] std::expected<void, StarException> LuaStackValue::set(K const& key, V&& value) const {
    return set_field(key, std::forward<V>(value), false);
  }
  template <typename K, typename V>
  [[nodiscard]] std::expected<void, StarException> LuaStackValue::raw_set(K const& key, V&& value) const {
    return set_field(key, std::forward<V>(value), true);
  }
  template <typename Function>
  [[nodiscard]] std::expected<void, StarException> LuaStackValue::iterate(Function&& function) const {
    auto& state = m_stack.state();
    LuaDetail::LuaStackRestore restore(state);
    if (!lua_checkstack(&state, 6))
      return std::unexpected(LuaException("Exhausted the Lua stack while iterating a table"));
    lua_pushnil(&state);
    int cursor = lua_gettop(&state);
    while (true) {
      lua_pushvalue(&state, m_index);
      lua_pushvalue(&state, cursor);
      auto next = m_stack.call(+[](lua_State* s) { return lua_next(s, 1) ? 2 : 0; }, 2, LUA_MULTRET);
      if (!next)
        return std::unexpected(std::move(next.error()));
      lua_remove(&state, cursor);
      if (lua_gettop(&state) < cursor)
        return {};
      auto key = m_stack.value(cursor);
      auto value = m_stack.value(cursor + 1);
      if (auto valid = key.validate(); !valid)
        return std::unexpected(std::move(valid.error()));
      if (auto valid = value.validate(); !valid)
        return std::unexpected(std::move(valid.error()));
      auto cont = LuaDetail::invoke_iterator(function, key, value);
      if (!cont)
        return std::unexpected(std::move(cont.error()));
      if (!*cont)
        return {};
      lua_pop(&state, 1);
    }
  }

  template <typename Container>
  [[nodiscard]] std::expected<void, StarException> LuaStack::push_array(Container const& values) const {
    if (values.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
      return std::unexpected(LuaException("Too many entries for a Lua array table"));
    auto table = create_table(static_cast<int>(values.size()), 0);
    if (!table)
      return std::unexpected(std::move(table.error()));
    LuaInt index = 0;
    for (auto const& value : values) {
      if (auto set = table->raw_set(++index, value); !set)
        return std::unexpected(std::move(set.error()));
    }
    return {};
  }
  template <typename Container>
  [[nodiscard]] std::expected<void, StarException> LuaStack::push_map(Container const& values) const {
    if (values.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
      return std::unexpected(LuaException("Too many entries for a Lua table"));
    auto table = create_table(0, static_cast<int>(values.size()));
    if (!table)
      return std::unexpected(std::move(table.error()));
    for (auto const& pair : values) {
      if (auto set = table->raw_set(pair.first, pair.second); !set)
        return std::unexpected(std::move(set.error()));
    }
    return {};
  }

  template <typename T> [[nodiscard]] std::expected<void, StarException> LuaStack::push_userdata(T value) const {
    if (auto registered = m_engine.register_userdata_type<T>(*this); !registered)
      return std::unexpected(std::move(registered.error()));
    if (!lua_checkstack(&m_state, 2))
      return std::unexpected(LuaException("Exhausted the Lua stack while creating userdata"));
    auto storage = m_engine.allocate_userdata(m_state, sizeof(T));
    if (!storage)
      return std::unexpected(std::move(storage.error()));
    new (storage->data()) T(std::move(value));
    lua_rawgeti(&m_state, LUA_REGISTRYINDEX, m_engine.m_registered_user_data_types.value(type_key<T>(), LUA_NOREF));
    lua_setmetatable(&m_state, -2);
    return {};
  }
  template <typename T> [[nodiscard]] std::expected<bool, StarException> LuaStackValue::is_userdata() const {
    if (!is<LuaUserData>())
      return false;
    auto& engine = m_stack.engine();
    int reference = engine.m_registered_user_data_types.value(type_key<T>(), LUA_NOREF);
    if (reference == LUA_NOREF)
      return false;
    auto& state = m_stack.state();
    LuaDetail::LuaStackRestore restore(state);
    if (!lua_checkstack(&state, 2))
      return std::unexpected(LuaException("Exhausted the Lua stack while checking userdata"));
    if (!lua_getmetatable(&state, m_index))
      return std::unexpected(LuaException("Userdata missing metatable in userDataIsType"));
    lua_rawgeti(&state, LUA_REGISTRYINDEX, reference);
    return lua_rawequal(&state, -1, -2) != 0;
  }
  template <typename T>
  [[nodiscard]] std::expected<std::reference_wrapper<T>, StarException> LuaStackValue::userdata() const {
    auto matches = is_userdata<T>();
    if (!matches)
      return std::unexpected(std::move(matches.error()));
    if (!*matches)
      return std::unexpected(
        LuaConversionException::format("Improper conversion from userdata to type {}", type_name<T>()));
    return std::ref(*static_cast<T*>(lua_touserdata(&m_stack.state(), m_index)));
  }
inline LuaReference::LuaReference(LuaDetail::LuaHandle handle) : m_handle(std::move(handle)) {}

inline bool LuaReference::operator==(LuaReference const& rhs) const {
  if (!m_handle.reference || !rhs.m_handle.reference)
    return m_handle.reference == rhs.m_handle.reference;
  return std::tie(m_handle.reference->engine, m_handle.reference->handle_index)
      == std::tie(rhs.m_handle.reference->engine, rhs.m_handle.reference->handle_index);
}

inline LuaEngine& LuaReference::engine() const {
  return *m_handle.reference->engine;
}

inline int LuaReference::handle_index() const {
  return m_handle.reference ? m_handle.reference->handle_index : 0;
}


inline std::size_t LuaString::length() const {
  return engine().string_length(handle_index());
}

inline String LuaString::to_string() const {
  return engine().string(handle_index());
}

inline StringView LuaString::view() const {
  return engine().string_view(handle_index());
}

inline bool operator==(LuaString const& s1, LuaString const& s2) {
  return s1.view() == s2.view();
}

inline bool operator==(LuaString const& s1, std::string_view s2) {
  return s1.view() == s2;
}

inline bool operator==(LuaString const& s1, std::string const& s2) {
  return s1.view() == s2;
}

inline bool operator==(LuaString const& s1, String const& s2) {
  return s1.view() == s2;
}

inline bool operator==(std::string_view s1, LuaString const& s2) {
  return s2.view() == s1;
}

inline bool operator==(std::string const& s1, LuaString const& s2) {
  return s2.view() == s1;
}

inline bool operator==(String const& s1, LuaString const& s2) {
  return s2.view() == s1;
}

template <typename T, typename K>
[[nodiscard]] std::expected<T, StarException> LuaTable::get(K const& key) const {
  auto& eng = engine();
  LuaDetail::LuaStackRestore restore(*eng.m_state);
  if (!lua_checkstack(eng.m_state.get(), 1))
    return std::unexpected(LuaException("Exhausted the Lua stack while accessing a table"));
  eng.push_handle(*eng.m_state, handle_index());
  return LuaStack(eng, *eng.m_state).value(-1).template get<T>(key);
}
template <typename T>
[[nodiscard]] std::expected<T, StarException> LuaTable::get(std::string_view key) const {
  return get<T, std::string_view>(key);
}
template <typename T, typename K>
[[nodiscard]] std::expected<void, StarException> LuaTable::set(K const& key, T&& value) const {
  auto& eng = engine();
  LuaDetail::LuaStackRestore restore(*eng.m_state);
  if (!lua_checkstack(eng.m_state.get(), 1))
    return std::unexpected(LuaException("Exhausted the Lua stack while accessing a table"));
  eng.push_handle(*eng.m_state, handle_index());
  return LuaStack(eng, *eng.m_state).value(-1).set(key, std::forward<T>(value));
}
template <typename T>
[[nodiscard]] std::expected<void, StarException> LuaTable::set(std::string_view key, T&& value) const {
  return set<T, std::string_view>(key, std::forward<T>(value));
}
template <typename K>
[[nodiscard]] std::expected<bool, StarException> LuaTable::contains(K const& key) const {
  auto& eng = engine();
  LuaDetail::LuaStackRestore restore(*eng.m_state);
  if (!lua_checkstack(eng.m_state.get(), 1))
    return std::unexpected(LuaException("Exhausted the Lua stack while accessing a table"));
  eng.push_handle(*eng.m_state, handle_index());
  return LuaStack(eng, *eng.m_state).value(-1).contains(key);
}
template <typename K>
[[nodiscard]] std::expected<void, StarException> LuaTable::remove(K const& key) const {
  return set(key, lua_nil);
}
template <LuaDetail::signature_deducible Function>
[[nodiscard]] std::expected<void, StarException> LuaTable::iterate(Function&& function) const {
  auto& eng = engine();
  LuaDetail::LuaStackRestore restore(*eng.m_state);
  if (!lua_checkstack(eng.m_state.get(), 1))
    return std::unexpected(LuaException("Exhausted the Lua stack while accessing a table"));
  eng.push_handle(*eng.m_state, handle_index());
  return LuaStack(eng, *eng.m_state)
    .value(-1)
    .iterate(LuaDetail::wrap_table_iterator(eng, std::forward<Function>(function)));
}
template <typename Return, typename... Args, typename Function>
[[nodiscard]] std::expected<void, StarException> LuaTable::iterate_with_signature(Function&& function) const {
  auto& eng = engine();
  LuaDetail::LuaStackRestore restore(*eng.m_state);
  if (!lua_checkstack(eng.m_state.get(), 1))
    return std::unexpected(LuaException("Exhausted the Lua stack while accessing a table"));
  eng.push_handle(*eng.m_state, handle_index());
  return LuaStack(eng, *eng.m_state)
    .value(-1)
    .iterate(LuaDetail::wrap_table_iterator_with_signature<Return, Args...>(eng, std::forward<Function>(function)));
}
template <typename T, typename K>
[[nodiscard]] std::expected<T, StarException> LuaTable::raw_get(K const& key) const {
  auto& eng = engine();
  LuaDetail::LuaStackRestore restore(*eng.m_state);
  if (!lua_checkstack(eng.m_state.get(), 1))
    return std::unexpected(LuaException("Exhausted the Lua stack while accessing a table"));
  eng.push_handle(*eng.m_state, handle_index());
  return LuaStack(eng, *eng.m_state).value(-1).template raw_get<T>(key);
}
template <typename T>
[[nodiscard]] std::expected<T, StarException> LuaTable::raw_get(std::string_view key) const {
  return raw_get<T, std::string_view>(key);
}
template <typename T, typename K>
[[nodiscard]] std::expected<void, StarException> LuaTable::raw_set(K const& key, T&& value) const {
  auto& eng = engine();
  LuaDetail::LuaStackRestore restore(*eng.m_state);
  if (!lua_checkstack(eng.m_state.get(), 1))
    return std::unexpected(LuaException("Exhausted the Lua stack while accessing a table"));
  eng.push_handle(*eng.m_state, handle_index());
  return LuaStack(eng, *eng.m_state).value(-1).raw_set(key, std::forward<T>(value));
}
template <typename T>
[[nodiscard]] std::expected<void, StarException> LuaTable::raw_set(std::string_view key, T&& value) const {
  return raw_set<T, std::string_view>(key, std::forward<T>(value));
}

template <typename Ret, typename... Args>
[[nodiscard]] std::expected<Ret, StarException> LuaFunction::invoke(Args const&... args) const {
  return engine().template call_function<Ret>(handle_index(), args...);
}

template <typename Ret, typename... Args>
[[nodiscard]] std::expected<std::optional<Ret>, StarException> LuaThread::resume(Args const&... args) const {
  return engine().template resume_thread<Ret>(handle_index(), args...);
}

[[nodiscard]] inline std::expected<void, StarException> LuaThread::push_function(LuaFunction const& func) const {
  if (&engine() != &func.engine())
    return std::unexpected(LuaException("Cannot push a function from another Lua engine"));
  return engine().thread_push_function(handle_index(), func.handle_index());
}

inline LuaThread::Status LuaThread::status() const {
  return engine().thread_status(handle_index());
}

template <typename T>
[[nodiscard]] std::expected<bool, StarException> LuaUserData::is() const {
  return engine().userdata_is_type<T>(handle_index());
}

template <typename T>
[[nodiscard]] std::expected<std::reference_wrapper<T>, StarException> LuaUserData::get() const {
  return engine().get_userdata<T>(handle_index());
}

template <LuaDetail::signature_deducible Function>
[[nodiscard]] std::expected<void, StarException> LuaCallbacks::register_callback(String name, Function&& func) {
  if (!m_callbacks.insert(name, LuaDetail::wrap_function(std::forward<Function>(func))).second)
    return std::unexpected(LuaException::format("Lua callback '{}' was registered twice", name));
  return {};
}

template <typename Return, typename... Args, typename Function>
[[nodiscard]] std::expected<void, StarException> LuaCallbacks::register_callback_with_signature(String name, Function&& func) {
  if (!m_callbacks.insert(name, LuaDetail::wrap_function_with_signature<Return, Args...>(std::forward<Function>(func))).second)
    return std::unexpected(LuaException::format("Lua callback '{}' was registered twice", name));
  return {};
}

template <typename T>
template <LuaDetail::signature_deducible Function>
[[nodiscard]] std::expected<void, StarException> LuaMethods<T>::register_method(String name, Function&& func) {
  if (!m_methods.insert(name, LuaDetail::wrap_method(std::forward<Function>(func))).second)
    return std::unexpected(LuaException::format("Lua method '{}' was registered twice", name));
  return {};
}

template <typename T>
template <typename Return, typename... Args, typename Function>
[[nodiscard]] std::expected<void, StarException> LuaMethods<T>::register_method_with_signature(String name, Function&& func) {
  if (!m_methods.insert(name, LuaDetail::wrap_method_with_signature<Return, Args...>(std::forward<Function>(func))).second)
    return std::unexpected(LuaException::format("Lua method '{}' was registered twice", name));
  return {};
}

template <typename T>
StringMap<LuaDetail::LuaWrappedFunction> const& LuaMethods<T>::methods() const {
  return m_methods;
}

template <LuaDetail::lua_readable T>
[[nodiscard]] std::expected<T, StarException> LuaContext::get_path(std::string_view path) const {
  auto& eng = engine();
  LuaDetail::LuaStackRestore restore(*eng.m_state);
  if (auto pushed = eng.push_context_path(handle_index(), path); !pushed)
    return std::unexpected(std::move(pushed.error()));
  return LuaStack(eng, *eng.m_state).read<T>(-1);
}

template <LuaDetail::lua_pushable T>
[[nodiscard]] std::expected<void, StarException> LuaContext::set_path(std::string_view path, T&& value) {
  auto& eng = engine();
  LuaDetail::LuaStackRestore restore(*eng.m_state);
  LuaStack stack(eng, *eng.m_state);
  if (auto pushed = stack.push(std::forward<T>(value)); !pushed)
    return std::unexpected(std::move(pushed.error()));
  if (auto valid = stack.value(-1).validate(); !valid)
    return std::unexpected(std::move(valid.error()));
  return eng.set_context_path(handle_index(), path);
}

template <typename Ret>
[[nodiscard]] std::expected<Ret, StarException> LuaContext::eval(String const& lua) {
  auto& eng = engine();
  LuaDetail::LuaStackRestore restore(*eng.m_state);
  auto result = eng.context_eval(handle_index(), lua, LuaDetail::LuaReturn<Ret>::count);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return LuaDetail::LuaReturn<Ret>::read(LuaStack(eng, *eng.m_state), restore.top + 1, *result);
}

template <LuaDetail::lua_call_result Ret, LuaDetail::lua_call_argument... Args>
[[nodiscard]] std::expected<Ret, StarException> LuaContext::invoke_path(std::string_view path, Args const&... args) const {
  auto& eng = engine();
  LuaDetail::LuaStackRestore restore(*eng.m_state);
  if (auto pushed = eng.push_context_path(handle_index(), path); !pushed)
    return std::unexpected(std::move(pushed.error()));
  if (!lua_isfunction(eng.m_state.get(), -1))
    return std::unexpected(LuaException::format("Lua path '{}' is not a function", path));
  auto count = eng.push_arguments(*eng.m_state, args...);
  if (!count)
    return std::unexpected(std::move(count.error()));
  return eng.template call_stack<Ret>(*eng.m_state, static_cast<int>(*count));
}

template <LuaDetail::lua_call_result Ret, LuaDetail::lua_call_argument... Args>
  requires (!std::is_void_v<Ret>)
[[nodiscard]] std::expected<std::optional<Ret>, StarException>
LuaContext::invoke_path_if_present(std::string_view path, Args const&... args) const {
  auto& eng = engine();
  LuaDetail::LuaStackRestore restore(*eng.m_state);
  if (auto pushed = eng.push_context_path(handle_index(), path); !pushed)
    return std::unexpected(std::move(pushed.error()));
  if (!lua_isfunction(eng.m_state.get(), -1)) {
    if (lua_isnil(eng.m_state.get(), -1))
      return std::optional<Ret>{};
    return std::unexpected(LuaConversionException::format("Lua path '{}' is not a function", path));
  }
  auto count = eng.push_arguments(*eng.m_state, args...);
  if (!count)
    return std::unexpected(std::move(count.error()));
  auto result = eng.template call_stack<Ret>(*eng.m_state, static_cast<int>(*count));
  if (!result)
    return std::unexpected(std::move(result.error()));
  return std::optional<Ret>(std::in_place, std::move(*result));
}

template <typename T>
[[nodiscard]] std::expected<LuaValue, StarException> LuaContext::lua_from(T&& t) {
  return engine().lua_from(std::forward<T>(t));
}

template <typename T>
[[nodiscard]] std::expected<LuaValue, StarException> LuaContext::lua_from(T const& t) {
  return engine().lua_from(t);
}

template <typename T>
[[nodiscard]] std::expected<std::optional<T>, StarException> LuaContext::lua_maybe_to(LuaValue&& v) {
  return engine().lua_maybe_to<T>(std::move(v));
}

template <typename T>
[[nodiscard]] std::expected<std::optional<T>, StarException> LuaContext::lua_maybe_to(LuaValue const& v) {
  return engine().lua_maybe_to<T>(v);
}

template <typename T>
[[nodiscard]] std::expected<T, StarException> LuaContext::lua_to(LuaValue&& v) {
  return engine().lua_to<T>(std::move(v));
}

template <typename T>
[[nodiscard]] std::expected<T, StarException> LuaContext::lua_to(LuaValue const& v) {
  return engine().lua_to<T>(v);
}

template <typename Container>
[[nodiscard]] std::expected<LuaTable, StarException> LuaContext::create_table(Container const& map) {
  return engine().create_table(map);
}

template <typename Container>
[[nodiscard]] std::expected<LuaTable, StarException> LuaContext::create_array_table(Container const& array) {
  return engine().create_array_table(array);
}

template <LuaDetail::signature_deducible Function>
[[nodiscard]] std::expected<LuaFunction, StarException> LuaContext::create_function(Function&& func) {
  return engine().create_function(std::forward<Function>(func));
}

template <typename Return, typename... Args, typename Function>
[[nodiscard]] std::expected<LuaFunction, StarException> LuaContext::create_function_with_signature(Function&& func) {
  return engine().create_function_with_signature<Return, Args...>(std::forward<Function>(func));
}

template <typename T>
[[nodiscard]] std::expected<LuaUserData, StarException> LuaContext::create_userdata(T t) {
  return engine().create_userdata(std::move(t));
}

template <typename T>
[[nodiscard]] std::expected<LuaMethods<T>, StarException> LuaUserDataMethods<T>::make() {
  return LuaMethods<T>();
}

template <typename T>
[[nodiscard]] std::expected<void, StarException> LuaUserDataConverter<T>::from(LuaStack stack, T t) {
  return stack.push_userdata(std::move(t));
}

template <typename T>
[[nodiscard]] std::expected<std::optional<T>, StarException> LuaUserDataConverter<T>::to(LuaStackValue v) {
  auto matches = v.is_userdata<T>();
  if (!matches)
    return std::unexpected(std::move(matches.error()));
  if (!*matches)
    return std::nullopt;
  auto value = v.userdata<T>();
  if (!value)
    return std::unexpected(std::move(value.error()));
  return std::optional<T>(std::in_place, value->get());
}

template <typename T>
[[nodiscard]] std::expected<LuaValue, StarException> LuaEngine::lua_from(T&& t) {
  if constexpr (std::is_same_v<std::remove_cvref_t<T>, LuaValue> || LuaDetail::lua_value_alternative<T>) {
    return LuaValue(std::forward<T>(t));
  } else {
    LuaDetail::LuaStackRestore restore(*m_state);
    auto pushed = LuaStack(*this, *m_state).push(std::forward<T>(t));
    if (!pushed)
      return std::unexpected(std::move(pushed.error()));
    return pop_lua_value(*m_state);
  }
}

template <typename T>
[[nodiscard]] std::expected<LuaValue, StarException> LuaEngine::lua_from(T const& t) {
  if constexpr (std::is_same_v<T, LuaValue> || LuaDetail::lua_value_alternative<T>) {
    return LuaValue(t);
  } else {
    LuaDetail::LuaStackRestore restore(*m_state);
    auto pushed = LuaStack(*this, *m_state).push(t);
    if (!pushed)
      return std::unexpected(std::move(pushed.error()));
    return pop_lua_value(*m_state);
  }
}

template <typename T>
[[nodiscard]] std::expected<std::optional<T>, StarException> LuaEngine::lua_maybe_to(LuaValue&& v) {
  if constexpr (std::is_same_v<T, LuaValue>) {
    return std::optional<T>(std::in_place, std::move(v));
  } else {
    if constexpr (LuaDetail::lua_value_alternative<T>) {
      if (std::holds_alternative<T>(v))
        return std::optional<T>(std::in_place, std::get<T>(std::move(v)));
    }
    return lua_maybe_to<T>(std::as_const(v));
  }
}

template <typename T>
[[nodiscard]] std::expected<std::optional<T>, StarException> LuaEngine::lua_maybe_to(LuaValue const& v) {
  if constexpr (std::is_same_v<T, LuaValue>) {
    return std::optional<T>(std::in_place, v);
  } else {
    if constexpr (LuaDetail::lua_value_alternative<T>) {
      if (std::holds_alternative<T>(v))
        return std::optional<T>(std::in_place, std::get<T>(v));
    }
    LuaDetail::LuaStackRestore restore(*m_state);
    if (auto pushed = push_lua_value(*m_state, v); !pushed)
      return std::unexpected(std::move(pushed.error()));
    return LuaStack(*this, *m_state).maybe_read<T>(-1);
  }
}

template <typename T>
[[nodiscard]] std::expected<T, StarException> LuaEngine::lua_to(LuaValue&& v) {
  auto result = lua_maybe_to<T>(std::move(v));
  if (!result)
    return std::unexpected(std::move(result.error()));
  if (!*result)
    return std::unexpected(conversion_error<T>());
  return std::move(**result);
}

template <typename T>
[[nodiscard]] std::expected<T, StarException> LuaEngine::lua_to(LuaValue const& v) {
  auto result = lua_maybe_to<T>(v);
  if (!result)
    return std::unexpected(std::move(result.error()));
  if (!*result)
    return std::unexpected(conversion_error<T>());
  return std::move(**result);
}

template <typename Container>
[[nodiscard]] std::expected<LuaTable, StarException> LuaEngine::create_table(Container const& map) {
  LuaDetail::LuaStackRestore restore(*m_state);
  LuaStack stack(*this, *m_state);
  if (auto pushed = stack.push_map(map); !pushed)
    return std::unexpected(std::move(pushed.error()));
  return stack.read<LuaTable>(-1);
}

template <typename Container>
[[nodiscard]] std::expected<LuaTable, StarException> LuaEngine::create_array_table(Container const& array) {
  LuaDetail::LuaStackRestore restore(*m_state);
  LuaStack stack(*this, *m_state);
  if (auto pushed = stack.push_array(array); !pushed)
    return std::unexpected(std::move(pushed.error()));
  return stack.read<LuaTable>(-1);
}

template <LuaDetail::signature_deducible Function>
[[nodiscard]] std::expected<LuaFunction, StarException> LuaEngine::create_function(Function&& func) {
  return create_wrapped_function(LuaDetail::wrap_function(std::forward<Function>(func)));
}

template <typename Return, typename... Args, typename Function>
[[nodiscard]] std::expected<LuaFunction, StarException> LuaEngine::create_function_with_signature(Function&& func) {
  return create_wrapped_function(LuaDetail::wrap_function_with_signature<Return, Args...>(std::forward<Function>(func)));
}

template <typename Ret, typename... Args>
[[nodiscard]] std::expected<Ret, StarException> LuaEngine::call_function(int handle_index, Args const&... args) {
  LuaDetail::LuaStackRestore restore(*m_state);
  if (!lua_checkstack(m_state.get(), 1))
    return std::unexpected(LuaException("Lua stack overflow while calling function"));
  push_handle(*m_state, handle_index);
  auto count = push_arguments(*m_state, args...);
  if (!count)
    return std::unexpected(std::move(count.error()));
  return call_stack<Ret>(*m_state, static_cast<int>(*count));
}

template <typename Ret>
[[nodiscard]] std::expected<Ret, StarException> LuaEngine::call_stack(lua_State& state, int nargs) {
  int first = lua_gettop(&state) - nargs;
  constexpr int count = LuaDetail::LuaReturn<Ret>::count;
  if (!lua_checkstack(&state, count == LUA_MULTRET ? 1 : count + 1))
    return std::unexpected(LuaException("Exhausted the Lua stack while reserving return values"));
  if (auto entered = increment_recursion_level(); !entered)
    return std::unexpected(std::move(entered.error()));
  int result = pcall_with_traceback(state, nargs, LuaDetail::LuaReturn<Ret>::count);
  decrement_recursion_level();
  if (auto called = handle_error(state, result); !called)
    return std::unexpected(std::move(called.error()));
  return LuaDetail::LuaReturn<Ret>::read(LuaStack(*this, state), first, lua_gettop(&state) - first + 1);
}

template <typename Ret, typename... Args>
[[nodiscard]] std::expected<std::optional<Ret>, StarException> LuaEngine::resume_thread(int handle_index,
                                                                                       Args const&... args) {
  LuaDetail::LuaStackRestore restore(*m_state);
  if (!lua_checkstack(m_state.get(), 4))
    return std::unexpected(LuaException("Lua stack overflow while resuming thread"));
  push_handle(*m_state, handle_index);
  lua_State& state = *lua_tothread(m_state.get(), -1);
  lua_pop(m_state.get(), 1);
  LuaDetail::LuaStackRestore restore_thread(state);
  if (lua_status(&state) != LUA_YIELD && lua_gettop(&state) == 0)
    return std::unexpected(LuaException("cannot resume a dead or errored thread"));
  auto count = push_arguments(state, args...);
  if (!count)
    return std::unexpected(std::move(count.error()));
  if (auto entered = increment_recursion_level(); !entered)
    return std::unexpected(std::move(entered.error()));
  release_handles(state);
  begin_diagnostic_scope();
  int results = 0;
  int result = lua_resume(&state, m_state.get(), static_cast<int>(*count), &results);
  release_handles(state);
  decrement_recursion_level();
  restore_thread.top = 0;
  if (result != LUA_OK && result != LUA_YIELD)
    propagate_error_with_traceback(state, *m_state);
  end_diagnostic_scope();
  if (result != LUA_OK && result != LUA_YIELD) {
    if (auto called = handle_error(*m_state, result); !called)
      return std::unexpected(std::move(called.error()));
  }
  int first = lua_gettop(&state) - results + 1;
  restore_thread.top = first - 1;
  auto value = LuaDetail::LuaReturn<Ret>::read(LuaStack(*this, state), first, results);
  if (!value)
    return std::unexpected(std::move(value.error()));
  return std::optional<Ret>(std::in_place, std::move(*value));
}

template <typename T>
[[nodiscard]] std::expected<void, StarException> LuaEngine::register_userdata_type(LuaStack stack) {
  if (m_registered_user_data_types.contains(type_key<T>()))
    return {};
  auto methods = LuaUserDataMethods<T>::make();
  if (!methods)
    return std::unexpected(std::move(methods.error()));

  LuaDetail::LuaStackRestore restore(stack.state());
  auto table = stack.create_table();
  if (!table)
    return std::unexpected(std::move(table.error()));
  if (auto index = table->raw_set("__index", *table); !index)
    return index;
  if (auto protection = table->raw_set("__metatable", false); !protection)
    return protection;
  if (!lua_checkstack(&stack.state(), 1))
    return std::unexpected(LuaException("Exhausted the Lua stack while installing a userdata finalizer"));
  lua_pushcfunction(&stack.state(), [](lua_State* state) {
    static_cast<T*>(lua_touserdata(state, 1))->~T();
    return 0;
  });
  if (auto finalizer = table->raw_set("__gc", stack.value(-1)); !finalizer)
    return finalizer;
  stack.pop();
  for (auto const& [name, method] : methods->methods()) {
    if (auto installed = table->raw_set(name, method); !installed)
      return installed;
  }

  auto reference = pop_handle(stack.state());
  if (!reference)
    return std::unexpected(std::move(reference.error()));
  auto added = m_registered_user_data_types.add(type_key<T>(), *reference);
  if (!added) {
    destroy_handle(*reference);
    return std::unexpected(std::move(added.error()));
  }
  return {};
}

template <typename T>
[[nodiscard]] std::expected<LuaUserData, StarException> LuaEngine::create_userdata(T t) {
  LuaDetail::LuaStackRestore restore(*m_state);
  auto pushed = LuaStack(*this, *m_state).push_userdata(std::move(t));
  if (!pushed)
    return std::unexpected(std::move(pushed.error()));
  auto reference = pop_handle(*m_state);
  if (!reference)
    return std::unexpected(std::move(reference.error()));
  return LuaUserData(LuaDetail::LuaHandle(LuaEnginePtr(*this), *reference));
}

template <typename T, typename K>
[[nodiscard]] std::expected<T, StarException> LuaEngine::get_global(K const& key) {
  LuaDetail::LuaStackRestore restore(*m_state);
  if (!lua_checkstack(m_state.get(), 1))
    return std::unexpected(LuaException("Lua stack overflow while reading global"));
  push_handle(*m_state, m_script_default_env_registry_id);
  return LuaStack(*this, *m_state).value(-1).template raw_get<T>(key);
}

template <typename T>
[[nodiscard]] std::expected<T, StarException> LuaEngine::get_global(std::string_view key) {
  return get_global<T, std::string_view>(key);
}

template <typename T, typename K>
[[nodiscard]] std::expected<void, StarException> LuaEngine::set_global(K const& key, T&& value) {
  LuaDetail::LuaStackRestore restore(*m_state);
  if (!lua_checkstack(m_state.get(), 1))
    return std::unexpected(LuaException("Lua stack overflow while writing global"));
  push_handle(*m_state, m_script_default_env_registry_id);
  return LuaStack(*this, *m_state).value(-1).raw_set(key, std::forward<T>(value));
}

template <typename T>
[[nodiscard]] std::expected<void, StarException> LuaEngine::set_global(std::string_view key, T&& value) {
  return set_global<T, std::string_view>(key, std::forward<T>(value));
}

template <typename T>
[[nodiscard]] std::expected<bool, StarException> LuaEngine::userdata_is_type(int handle_index) {
  int type_ref = m_registered_user_data_types.value(type_key<T>(), LUA_NOREF);
  if (type_ref == LUA_NOREF)
    return false;
  LuaDetail::LuaStackRestore restore(*m_state);
  if (!lua_checkstack(m_state.get(), 3))
    return std::unexpected(LuaException("Lua stack overflow while checking userdata"));
  push_handle(*m_state, handle_index);
  if (lua_getmetatable(m_state.get(), -1) == 0)
    return std::unexpected(LuaException("Userdata missing metatable in userDataIsType"));
  lua_rawgeti(m_state.get(), LUA_REGISTRYINDEX, type_ref);
  return lua_rawequal(m_state.get(), -1, -2) != 0;
}

template <typename T>
[[nodiscard]] std::expected<std::reference_wrapper<T>, StarException> LuaEngine::get_userdata(int handle_index) {
  int type_ref = m_registered_user_data_types.value(type_key<T>(), LUA_NOREF);
  if (type_ref == LUA_NOREF)
    return std::unexpected(LuaException::format("Cannot convert userdata type of {}, not registered", type_name<T>()));
  LuaDetail::LuaStackRestore restore(*m_state);
  if (!lua_checkstack(m_state.get(), 3))
    return std::unexpected(LuaException("Lua stack overflow while accessing userdata"));
  push_handle(*m_state, handle_index);
  void* userdata = lua_touserdata(m_state.get(), -1);
  if (lua_getmetatable(m_state.get(), -1) == 0)
    return std::unexpected(LuaException("Cannot get userdata from lua type, no metatable found"));
  lua_rawgeti(m_state.get(), LUA_REGISTRYINDEX, type_ref);
  if (!lua_rawequal(m_state.get(), -1, -2) || !userdata)
    return std::unexpected(LuaException::format("Improper conversion from userdata to type {}", type_name<T>()));
  return std::reference_wrapper<T>(*static_cast<T*>(userdata));
}

inline void LuaEngine::destroy_handle(int handle_index) {
  // Registry references are released later by the engine's handle cleanup.
  m_handle_free.push_back(handle_index);
}

template <typename T>
[[nodiscard]] std::expected<std::size_t, StarException> LuaEngine::push_argument(lua_State& state, T const& arg) {
  if (auto pushed = LuaStack(*this, state).push(arg); !pushed)
    return std::unexpected(std::move(pushed.error()));
  return 1;
}

template <typename T>
[[nodiscard]] std::expected<std::size_t, StarException> LuaEngine::push_argument(lua_State& state, LuaVariadic<T> const& args) {
  if (args.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    return std::unexpected(LuaException("Too many Lua arguments"));
  LuaDetail::LuaStackRestore restore(state);
  LuaStack stack(*this, state);
  for (auto const& arg : args) {
    if (auto pushed = stack.push(arg); !pushed)
      return std::unexpected(std::move(pushed.error()));
  }
  restore.top = lua_gettop(&state);
  return args.size();
}

[[nodiscard]] inline std::expected<std::size_t, StarException> LuaEngine::do_push_arguments(lua_State&) {
  return 0;
}

template <typename First, typename... Rest>
[[nodiscard]] std::expected<std::size_t, StarException> LuaEngine::do_push_arguments(lua_State& state, First const& first, Rest const&... rest) {
  auto first_size = push_argument(state, first);
  if (!first_size)
    return std::unexpected(std::move(first_size.error()));
  auto rest_size = do_push_arguments(state, rest...);
  if (!rest_size)
    return std::unexpected(std::move(rest_size.error()));
  if (*rest_size > static_cast<std::size_t>(std::numeric_limits<int>::max()) - *first_size)
    return std::unexpected(LuaException("Too many Lua arguments"));
  return *first_size + *rest_size;
}

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, StarException> LuaEngine::push_arguments(lua_State& state, Args const&... args) {
  if (sizeof...(args) > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    return std::unexpected(LuaException("Too many Lua arguments"));
  if (!lua_checkstack(&state, static_cast<int>(sizeof...(args))))
    return std::unexpected(LuaException("Lua stack overflow while pushing arguments"));
  LuaDetail::LuaStackRestore restore(state);
  auto count = do_push_arguments(state, args...);
  if (!count)
    return std::unexpected(std::move(count.error()));
  restore.top = lua_gettop(&state);
  return *count;
}

}

export module star.lua;

export namespace Star {
  using ::Star::LuaExecutionHost;
  template <>
  struct LuaConverter<bool>;

  template <>
  struct LuaConverter<char>;

  template <>
  struct LuaConverter<unsigned char>;

  template <>
  struct LuaConverter<short>;

  template <>
  struct LuaConverter<unsigned short>;

  template <>
  struct LuaConverter<long>;

  template <>
  struct LuaConverter<unsigned long>;

  template <>
  struct LuaConverter<int>;

  template <>
  struct LuaConverter<unsigned int>;

  template <>
  struct LuaConverter<long long>;

  template <>
  struct LuaConverter<unsigned long long>;

  template <>
  struct LuaConverter<float>;

  template <>
  struct LuaConverter<double>;

  template <>
  struct LuaConverter<String>;

  template <>
  struct LuaConverter<std::string>;

  template <>
  struct LuaConverter<std::string_view>;


  template <std::size_t s>
  struct LuaConverter<char[s]>;

  template <>
  struct LuaConverter<Directives>;

  template <>
  struct LuaConverter<LuaString>;

  template <>
  struct LuaConverter<LuaTable>;

  template <>
  struct LuaConverter<LuaFunction>;

  template <>
  struct LuaConverter<LuaDetail::LuaWrappedFunction>;

  template <>
  struct LuaConverter<LuaThread>;

  template <>
  struct LuaConverter<LuaUserData>;

  template <>
  struct LuaConverter<LuaValue>;

  template <typename T>
  struct LuaConverter<std::optional<T>>;

  template <typename T, typename Allocator>
  struct LuaConverter<List<T, Allocator>>;

  template <typename T, std::size_t max_size>
  struct LuaConverter<StaticList<T, max_size>>;

  template <typename T, std::size_t max_stack_size>
  struct LuaConverter<SmallList<T, max_stack_size>>;

  template <>
  struct LuaConverter<StringList>;

  template <set_container Container>
  struct LuaConverter<Container>;

  template <typename Key, typename Value, typename Compare, typename Allocator>
  struct LuaConverter<Map<Key, Value, Compare, Allocator>>;

  template <typename Key, typename Value, typename Hash, typename Equals, typename Allocator>
  struct LuaConverter<HashMap<Key, Value, Hash, Equals, Allocator>>;

  template <>
  struct LuaConverter<Json>;

  template <>
  struct LuaConverter<JsonObject>;

  template <>
  struct LuaConverter<JsonArray>;

  using ::Star::LuaBoolean;
  using ::Star::LuaCallbacks;
  using ::Star::LuaContext;
  using ::Star::LuaConversionException;
  using ::Star::LuaConversionExceptionTag;
  using ::Star::LuaEngine;
  using ::Star::LuaEnginePtr;
  using ::Star::LuaException;
  using ::Star::LuaExceptionTag;
  using ::Star::LuaFloat;
  using ::Star::LuaFunction;
  using ::Star::LuaHostError;
  using ::Star::LuaHostErrorTag;
  using ::Star::LuaIncompleteStatementException;
  using ::Star::LuaIncompleteStatementExceptionTag;
  using ::Star::LuaInstructionLimitReached;
  using ::Star::LuaInstructionLimitReachedTag;
  using ::Star::LuaInt;
  using ::Star::LuaMethods;
  using ::Star::lua_nil;
  using ::Star::LuaNilType;
  using ::Star::LuaRecursionLimitReached;
  using ::Star::LuaRecursionLimitReachedTag;
  using ::Star::LuaReference;
  using ::Star::LuaStack;
  using ::Star::LuaStackValue;
  using ::Star::LuaString;
  using ::Star::LuaTable;
  using ::Star::LuaThread;
  using ::Star::lua_tie;
  using ::Star::LuaTupleReturn;
  using ::Star::lua_tuple_return;
  using ::Star::lua_unpack;
  using ::Star::LuaUserData;
  using ::Star::LuaValue;
  using ::Star::LuaVariadic;
  using ::Star::print_lua_value;
  using ::Star::write_text;
  template <> struct LuaConverter<LuaStackValue>;
  using ::Star::LuaProfileEntry;
  using ::Star::LuaConverter;
  using ::Star::LuaUserDataMethods;
  using ::Star::LuaUserDataConverter;
  using ::Star::LuaIntConverter;
  using ::Star::LuaFloatConverter;
  using ::Star::LuaValueConverter;
  using ::Star::LuaMapConverter;
  using ::Star::LuaContainerConverter;
  using ::Star::operator<<;
  using ::Star::operator==;
  namespace LuaDetail {
  using ::Star::LuaDetail::ArgGet;
  using ::Star::LuaDetail::as_integer;
  using ::Star::LuaDetail::FunctionWrapper;
  using ::Star::LuaDetail::insert_json_metatable;
  using ::Star::LuaDetail::jarray;
  using ::Star::LuaDetail::jarray_create;
  using ::Star::LuaDetail::jcont_remove;
  using ::Star::LuaDetail::jcont_resize;
  using ::Star::LuaDetail::jcont_size;
  using ::Star::LuaDetail::jobject;
  using ::Star::LuaDetail::jobject_create;
  using ::Star::LuaDetail::json_container_to_table;
  using ::Star::LuaDetail::LuaHandle;
  using ::Star::LuaDetail::LuaReturn;
  using ::Star::LuaDetail::LuaStackRestore;
  using ::Star::LuaDetail::LuaWrappedFunction;
  using ::Star::LuaDetail::MethodWrapper;
  using ::Star::LuaDetail::raw_set_field;
  using ::Star::LuaDetail::shallow_copy;
  using ::Star::LuaDetail::TableIteratorWrapper;
  using ::Star::LuaDetail::to_wrapped_return;
  using ::Star::LuaDetail::wrap_function;
  using ::Star::LuaDetail::wrap_function_args;
  using ::Star::LuaDetail::wrap_function_with_signature;
  using ::Star::LuaDetail::wrap_method;
  using ::Star::LuaDetail::wrap_method_args;
  using ::Star::LuaDetail::wrap_method_with_signature;
  using ::Star::LuaDetail::wrap_table_iterator;
  using ::Star::LuaDetail::wrap_table_iterator_args;
  using ::Star::LuaDetail::wrap_table_iterator_with_signature;
  }
}
