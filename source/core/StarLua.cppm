module;
#include <lua.hpp>
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
  virtual void const* queryType(TypeKey key) const noexcept {
    return queryTypeOf<LuaExecutionHost>(this, key);
  }
  virtual std::string_view dynamicTypeName() const noexcept {
    return typeName<LuaExecutionHost>();
  }
};
typedef RefPtr<LuaEngine> LuaEnginePtr;

// General Lua diagnostic category.
struct LuaExceptionTag {
  static constexpr std::string_view name() { return "LuaException"; }
};
using LuaException = StarError<LuaExceptionTag, StarException>;

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

typedef Empty LuaNilType;
typedef bool LuaBoolean;
typedef lua_Integer LuaInt;
typedef lua_Number LuaFloat;
class LuaString;
class LuaTable;
class LuaFunction;
class LuaThread;
class LuaUserData;
typedef std::variant<LuaNilType, LuaBoolean, LuaInt, LuaFloat, LuaString, LuaTable, LuaFunction, LuaThread, LuaUserData> LuaValue;

// Used to wrap multiple return values from calling a lua function or to pass
// multiple values as arguments to a lua function from a container.  If this is
// used as an argument to a lua callback function, it must be the final
// argument of the function!
template <typename T>
class LuaVariadic : public List<T> {
public:
  using List<T>::List;
  LuaVariadic() = default;
  LuaVariadic(List<T> const& values) : List<T>(values) {}
  LuaVariadic(List<T>&& values) : List<T>(std::move(values)) {}
};

// Unpack a container and apply each of the arguments separately to a lua
// function, similar to lua's unpack.
template <typename Container>
LuaVariadic<typename std::decay<Container>::type::value_type> luaUnpack(Container&& c);

// Similar to LuaVariadic, but a tuple type so automatic per-entry type
// conversion is done.  This can only be used as the return value of a wrapped
// c++ function, or as a type for the return value of calling a lua function.
template <typename... Types>
class LuaTupleReturn : public std::tuple<Types...> {
public:
  typedef std::tuple<Types...> Base;

  explicit LuaTupleReturn(Types const&... args);
  template <typename... UTypes>
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
LuaTupleReturn<Types&...> luaTie(Types&... args);

// Constructs a LuaTupleReturn from the given arguments similar to make_tuple
template <typename... Types>
LuaTupleReturn<typename std::decay<Types>::type...> luaTupleReturn(Types&&... args);

// Synchronous stack borrows. Views never escape their call; use own() when a
// value must survive it. All allocating/metamethod operations stay protected.
class LuaStackValue;
class LuaStack {
public:
  LuaStack(LuaEngine& engine, lua_State* state) : m_engine(engine), m_state(state) {}
  LuaEngine& engine() const { return m_engine; }
  lua_State* state() const { return m_state; }
  int top() const { return lua_gettop(m_state); }
  void pop(int count = 1) const { lua_pop(m_state, count); }
  LuaStackValue value(int index) const;
  template <typename T> [[nodiscard]] std::expected<void, StarException> push(T&& value) const;
  template <typename T> [[nodiscard]] std::expected<T, StarException> read(int index) const;
  template <typename T> [[nodiscard]] std::expected<std::optional<T>, StarException> maybeRead(int index) const;
  [[nodiscard]] std::expected<LuaStackValue, StarException> createTable(int narr = 0, int nrec = 0) const;
  template <typename Container>
  [[nodiscard]] std::expected<void, StarException> pushArray(Container const& values) const;
  template <typename Container> [[nodiscard]] std::expected<void, StarException> pushMap(Container const& values) const;
  [[nodiscard]] std::expected<void, StarException> pushString(std::string_view value) const;
  template <typename T> [[nodiscard]] std::expected<void, StarException> pushUserData(T value) const;
  [[nodiscard]] std::expected<void, StarException> call(lua_CFunction function, int nargs, int nresults) const;
  // Execute source or bytecode in a borrowed environment on this same stack.
  [[nodiscard]] std::expected<void, StarException> load(LuaStackValue environment, std::span<char const> contents,
                                                        String const& name = {}) const;

private:
  LuaEngine& m_engine;
  lua_State* m_state;
};

class LuaStackValue {
public:
  LuaStackValue(LuaStack stack, int index) : m_stack(stack), m_index(lua_absindex(stack.state(), index)) {}
  LuaStack stack() const { return m_stack; }
  int index() const { return m_index; }
  template <typename T> bool is() const;
  template <typename T> T as() const;
  StringView stringView() const;
  String toString() const { return String(stringView().utf8()); }
  template <typename T> [[nodiscard]] std::expected<T, StarException> to() const;
  template <typename T> [[nodiscard]] std::expected<std::optional<T>, StarException> maybeTo() const;
  template <typename T = LuaValue, typename K> [[nodiscard]] std::expected<T, StarException> get(K const& key) const;
  template <typename T, typename K>
  [[nodiscard]] std::expected<std::optional<T>, StarException> maybeGet(K const& key) const;
  template <typename K> [[nodiscard]] std::expected<bool, StarException> contains(K const& key) const;
  template <typename T = LuaValue, typename K> [[nodiscard]] std::expected<T, StarException> rawGet(K const& key) const;
  template <typename K, typename V> [[nodiscard]] std::expected<void, StarException> set(K const& key, V&& value) const;
  template <typename K, typename V>
  [[nodiscard]] std::expected<void, StarException> rawSet(K const& key, V&& value) const;
  template <typename Function> [[nodiscard]] std::expected<void, StarException> iterate(Function&& function) const;
  [[nodiscard]] std::expected<LuaInt, StarException> length() const;
  [[nodiscard]] std::expected<LuaInt, StarException> rawLength() const;
  [[nodiscard]] std::expected<LuaValue, StarException> own() const;
  template <typename T> [[nodiscard]] std::expected<bool, StarException> isUserData() const;
  template <typename T> [[nodiscard]] std::expected<std::reference_wrapper<T>, StarException> userData() const;
  [[nodiscard]] std::expected<void, StarException> validate() const;

private:
  template <typename K> [[nodiscard]] std::expected<void, StarException> pushField(K const& key, bool raw) const;
  template <typename K, typename V>
  [[nodiscard]] std::expected<void, StarException> setField(K const& key, V&& value, bool raw) const;
  LuaStack m_stack;
  int m_index;
};

namespace LuaDetail {
[[nodiscard]] std::expected<void, StarException> insertJsonMetatable(LuaEngine& engine, LuaTable const& table,
                                                                     Json::Type type);
struct LuaRegistryReference {
  LuaRegistryReference(LuaEnginePtr engine, int handleIndex);
  LuaRegistryReference(LuaRegistryReference const&) = delete;
  LuaRegistryReference& operator=(LuaRegistryReference const&) = delete;
  ~LuaRegistryReference();

  LuaEnginePtr engine;
  int handleIndex;
};

  struct LuaHandle {
    LuaHandle(LuaEnginePtr engine, int handleIndex);
    ~LuaHandle() = default;

    LuaHandle(LuaHandle const&) = default;
    LuaHandle(LuaHandle&&) = default;
    LuaHandle& operator=(LuaHandle const&) = default;
    LuaHandle& operator=(LuaHandle&&) = default;

    std::shared_ptr<LuaRegistryReference> reference;
  };

  // Wrapped callbacks decode borrowed arguments and append their results.
  typedef std::function<std::expected<int, StarException>(LuaStack)> LuaWrappedFunction;
}

// Rendering may inspect tables and therefore reports recoverable Lua errors.
[[nodiscard]] std::expected<std::reference_wrapper<std::ostream>, StarException> operator<<(std::ostream& os, LuaValue const& value);
[[nodiscard]] std::expected<String, StarException> printLuaValue(LuaValue const& value);

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
  int handleIndex() const;

private:
  LuaDetail::LuaHandle m_handle;
};

class LuaString : public LuaReference {
public:
  using LuaReference::LuaReference;

  std::size_t length() const;

  String toString() const;
  StringView view() const;
};

bool operator==(LuaString const& s1, LuaString const& s2);
bool operator==(LuaString const& s1, std::string_view s2);
bool operator==(LuaString const& s1, std::string const& s2);
bool operator==(LuaString const& s1, String const& s2);
bool operator==(std::string_view s1, LuaString const& s2);
template <std::size_t N>
bool operator==(LuaString const& s1, char const (&s2)[N]) { return s1 == std::string_view(s2); }
template <std::size_t N>
bool operator==(char const (&s1)[N], LuaString const& s2) { return std::string_view(s1) == s2; }
bool operator==(std::string const& s1, LuaString const& s2);
bool operator==(String const& s1, LuaString const& s2);

// C++20 rewrites != from these.
bool operator==(LuaString const& s1, LuaString const& s2);

class LuaTable : public LuaReference {
public:
  using LuaReference::LuaReference;

  template <typename T = LuaValue, typename K>
  [[nodiscard]] std::expected<T, StarException> get(K key) const;
  template <typename T = LuaValue>
  [[nodiscard]] std::expected<T, StarException> get(std::string_view key) const;

  template <typename T, typename K>
  [[nodiscard]] std::expected<void, StarException> set(K key, T t) const;
  template <typename T>
  [[nodiscard]] std::expected<void, StarException> set(std::string_view key, T t) const;

  // Shorthand for get(path) != LuaNil
  template <typename K>
  [[nodiscard]] std::expected<bool, StarException> contains(K key) const;
  [[nodiscard]] std::expected<bool, StarException> contains(std::string_view key) const;

  // Shorthand for setting to LuaNil
  template <typename K>
  [[nodiscard]] std::expected<void, StarException> remove(K key) const;
  [[nodiscard]] std::expected<void, StarException> remove(std::string_view key) const;

  // Result of lua # operator
  [[nodiscard]] std::expected<LuaInt, StarException> length() const;

  // If iteration function returns bool, returning false signals stopping.
  template <typename Function>
  [[nodiscard]] std::expected<void, StarException> iterate(Function&& iterator) const;

  template <typename Return, typename... Args, typename Function>
  [[nodiscard]] std::expected<void, StarException> iterateWithSignature(Function&& func) const;

  [[nodiscard]] std::expected<std::optional<LuaTable>, StarException> getMetatable() const;
  [[nodiscard]] std::expected<void, StarException> setMetatable(LuaTable const& table) const;

  template <typename T = LuaValue, typename K>
  [[nodiscard]] std::expected<T, StarException> rawGet(K key) const;
  template <typename T = LuaValue>
  [[nodiscard]] std::expected<T, StarException> rawGet(std::string_view key) const;

  template <typename T, typename K>
  [[nodiscard]] std::expected<void, StarException> rawSet(K key, T t) const;
  template <typename T>
  [[nodiscard]] std::expected<void, StarException> rawSet(std::string_view key, T t) const;

  [[nodiscard]] std::expected<LuaInt, StarException> rawLength() const;
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
  [[nodiscard]] std::expected<void, StarException> pushFunction(LuaFunction const& func) const;
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

inline const LuaValue LuaNil = LuaValue();

class LuaCallbacks {
public:
  [[nodiscard]] std::expected<void, StarException> copyCallback(String srcName, String dstName);

  template <typename Function>
  [[nodiscard]] std::expected<void, StarException> registerCallback(String name, Function&& func);

  bool removeCallback(String name);

  template <typename Return, typename... Args, typename Function>
  [[nodiscard]] std::expected<void, StarException> registerCallbackWithSignature(String name, Function&& func);

  [[nodiscard]] std::expected<void, StarException> merge(LuaCallbacks const& callbacks);

  // Borrowed callbacks installed by an owner may outlive its Lua context.
  // Retired scopes reject even saved Lua function values before dereferencing.
  LuaCallbacks withLifetime(std::stop_token lifetime) const;

  StringMap<LuaDetail::LuaWrappedFunction> const& callbacks() const;

private:
  StringMap<LuaDetail::LuaWrappedFunction> m_callbacks;
};

template <typename T>
class LuaMethods {
public:
  template <typename Function>
  [[nodiscard]] std::expected<void, StarException> registerMethod(String name, Function&& func);

  template <typename Return, typename... Args, typename Function>
  [[nodiscard]] std::expected<void, StarException> registerMethodWithSignature(String name, Function&& func);

  StringMap<LuaDetail::LuaWrappedFunction> const& methods() const;

private:
  StringMap<LuaDetail::LuaWrappedFunction> m_methods;
};

// A single execution context from a LuaEngine that manages a (mostly) distinct
// lua environment.  Each LuaContext's global environment is separate and one
// LuaContext can (mostly) not affect any other.
class LuaContext : protected LuaTable {
public:
  typedef std::function<std::expected<void, StarException>(LuaStackValue, StringView)> RequireFunction;

  using LuaTable::LuaTable;

  using LuaTable::get;
  using LuaTable::set;
  using LuaTable::contains;
  using LuaTable::remove;
  using LuaTable::engine;
  using LuaTable::handleIndex;

  // Splits the path by '.' character, so can get / set values in tables inside
  // other tables.  If any table in the path is not a table but is accessed as
  // one, instead returns LuaNil.
  template <typename T = LuaValue>
  [[nodiscard]] std::expected<T, StarException> getPath(String path) const;
  // Shorthand for getPath != LuaNil
  [[nodiscard]] std::expected<bool, StarException> containsPath(String path) const;
  // Will create new tables if the key contains paths that are nil
  template <typename T>
  [[nodiscard]] std::expected<void, StarException> setPath(String path, T value);

  // Load the given code (either source or bytecode) into this context as a new
  // chunk.  It is not necessary to provide the name again if given bytecode.
  [[nodiscard]] std::expected<void, StarException> load(std::span<char const> contents, String const& name = {});
  template <std::size_t N>
  [[nodiscard]] std::expected<void, StarException> load(char const (&contents)[N], String const& name = {}) {
    return load(std::span(contents).first(N - 1), name);
  }
  [[nodiscard]] std::expected<void, StarException> load(String const& contents, String const& name = String());
  [[nodiscard]] std::expected<void, StarException> load(ByteArray const& contents, String const& name = String());

  // Evaluate a piece of lua code in this context, similar to the lua repl.
  // Can evaluate both expressions and statements.
  template <typename T = LuaValue>
  [[nodiscard]] std::expected<T, StarException> eval(String const& lua);

  // Override require with a synchronous callback borrowing its environment and
  // module name from the calling Lua stack.
  [[nodiscard]] std::expected<void, StarException> setRequireFunction(RequireFunction requireFunction);

  [[nodiscard]] std::expected<void, StarException> setCallbacks(String const& tableName, LuaCallbacks const& callbacks) const;

  // Resolve at every call, without promoting the function to a registry root.

  template <typename Ret = LuaValue, typename... Args>
  [[nodiscard]] std::expected<Ret, StarException> invokePath(String const& key, Args const&... args) const;
  template <typename Ret = LuaValue, typename... Args>
  [[nodiscard]] std::expected<std::optional<Ret>, StarException> invokePathIfPresent(String const& key,
                                                                                     Args const&... args) const;

  // For convenience, calls to LuaEngine conversion / create functions are
  // duplicated here.

  template <typename T>
  [[nodiscard]] std::expected<LuaValue, StarException> luaFrom(T&& t);
  template <typename T>
  [[nodiscard]] std::expected<LuaValue, StarException> luaFrom(T const& t);
  template <typename T>
  [[nodiscard]] std::expected<std::optional<T>, StarException> luaMaybeTo(LuaValue&& v);
  template <typename T>
  [[nodiscard]] std::expected<std::optional<T>, StarException> luaMaybeTo(LuaValue const& v);
  template <typename T>
  [[nodiscard]] std::expected<T, StarException> luaTo(LuaValue const& v);
  template <typename T>
  [[nodiscard]] std::expected<T, StarException> luaTo(LuaValue&& v);

  [[nodiscard]] std::expected<LuaString, StarException> createString(String const& str);
  [[nodiscard]] std::expected<LuaString, StarException> createString(std::string_view str);
  [[nodiscard]] std::expected<LuaString, StarException> createString(std::string const& str) { return createString(std::string_view(str)); }
  template <std::size_t N>
  [[nodiscard]] std::expected<LuaString, StarException> createString(char const (&str)[N]) { return createString(std::string_view(str)); }

  [[nodiscard]] std::expected<LuaTable, StarException> createTable();

  template <typename Container>
  [[nodiscard]] std::expected<LuaTable, StarException> createTable(Container const& map);

  template <typename Container>
  [[nodiscard]] std::expected<LuaTable, StarException> createArrayTable(Container const& array);

  template <typename Function>
  [[nodiscard]] std::expected<LuaFunction, StarException> createFunction(Function&& func);

  template <typename Return, typename... Args, typename Function>
  [[nodiscard]] std::expected<LuaFunction, StarException> createFunctionWithSignature(Function&& func);

  template <typename T>
  [[nodiscard]] std::expected<LuaUserData, StarException> createUserData(T t);
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
  unsigned sourceLine;
  // Name of the function, if it can be determined
  std::optional<String> name;
  // Scope of the function, if it can be determined
  std::optional<String> nameScope;
  // Time taken within this function itself
  std::int64_t selfTime;
  // Total time taken within this function or sub functions
  std::int64_t totalTime;
  // Calls from this function
  HashMap<std::tuple<String, unsigned>, std::shared_ptr<LuaProfileEntry>> calls;
};

// This class represents one execution engine in lua, holding a single
// lua_State.  Multiple contexts can be created, and they will have separate
// global environments and cannot affect each other.  Individual LuaEngines /
// LuaContexts are not thread safe, use one LuaEngine per thread.
class LuaEngine : public RefCounter {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<LuaEngine, RefCounter>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return typeName<LuaEngine>();
  }
  // If 'safe' is true, then creates a lua engine with all builtin lua
  // functions that can affect the real world disabled.
  [[nodiscard]] static std::expected<LuaEnginePtr, StarException> create(bool safe = true);

  ~LuaEngine();

  LuaEngine(LuaEngine const&) = delete;
  LuaEngine(LuaEngine&&) = default;

  LuaEngine& operator=(LuaEngine const&) = delete;
  LuaEngine& operator=(LuaEngine&&) = default;

  // Optional host capability. Core-only engines leave image resolution unset.
  using ImageResolver = std::function<std::expected<std::shared_ptr<Image const>, StarException>(String const&)>;
  void setImageResolver(ImageResolver resolver) { m_imageResolver = std::move(resolver); }
  ImageResolver const& imageResolver() const { return m_imageResolver; }
  void setExecutionHost(std::unique_ptr<LuaExecutionHost> host) { m_executionHost = std::move(host); }
  std::optional<std::reference_wrapper<LuaExecutionHost>> executionHost() const { return optionalRef(m_executionHost); }
  std::stop_token const& promiseLifetime() const { return m_promiseLifetime; }
  std::stop_token setPromiseLifetime(std::stop_token lifetime) {
    return std::exchange(m_promiseLifetime, std::move(lifetime));
  }

  // Set the instruction limit for computation sequences in the engine.  During
  // any function invocation, thread resume, or code evaluation, an instruction
  // counter will be started.  In the event that the instruction counter
  // becomes greater than the given limit, LuaInstructionLimitReached is returned. The
  // count is only reset when the initial entry into LuaEngine is returned,
  // recursive entries into LuaEngine accumulate the same instruction counter.
  // 0 disables the instruction limit.
  void setInstructionLimit(std::uint64_t instructionLimit = 0);
  std::uint64_t instructionLimit() const;

  // If profiling is enabled, then every 'measureInterval' instructions, the
  // function call stack will be recorded, and a summary of function timing can
  // be printed using profileReport
  void setProfilingEnabled(bool profilingEnabled);
  bool profilingEnabled() const;

  // Print a summary of the profiling data gathered since profiling was last
  // enabled.
  List<LuaProfileEntry> getProfile();

  // If an instruction limit is set or profiling is neabled, this field
  // describes the resolution of instruction count measurement, and affects the
  // accuracy of profiling and the instruction count limit.  Defaults to 1000
  void setInstructionMeasureInterval(unsigned measureInterval = 1000);
  unsigned instructionMeasureInterval() const;

  // Sets the LuaEngine recursion limit, limiting the number of times a
  // LuaEngine call may directly or inderectly trigger a call back into the
  // LuaEngine, preventing a C++ stack overflow.  0 disables the limit.
  void setRecursionLimit(unsigned recursionLimit = 0);
  unsigned recursionLimit() const;

  // Compile a given script into bytecode.  If name is given, then it will be
  // used as the internal name for the resulting chunk and will provide better
  // error messages.
  //
  // Unfortunately the only way to completely ensure that a single script will
  // execute in two separate contexts and truly be isolated is to compile the
  // script to bytecode and load once in each context as a separate chunk.
  [[nodiscard]] std::expected<ByteArray, StarException> compile(std::span<char const> contents, String const& name = {});
  template <std::size_t N>
  [[nodiscard]] std::expected<ByteArray, StarException> compile(char const (&contents)[N], String const& name = {}) {
    return compile(std::span(contents).first(N - 1), name);
  }
  [[nodiscard]] std::expected<ByteArray, StarException> compile(String const& contents, String const& name = String());
  [[nodiscard]] std::expected<ByteArray, StarException> compile(ByteArray const& contents, String const& name = String());

  // Optional host diagnostics for full Lua chunk sources. The resolver must
  // own its snapshots, not borrow a shorter-lived host or capture this engine.
  // Failures are ignored so diagnostics cannot replace a script error.
  using SourceResolver = std::function<std::expected<String, StarException>(String const&)>;
  void setSourceResolver(SourceResolver resolver);

  // Returns the debug info of the state.
  lua_Debug const& debugInfo(int level = 1, String const& what = "nSlu");

  // Generic from/to lua conversion, calls template specialization of
  // LuaConverter for actual conversion.
  template <typename T>
  [[nodiscard]] std::expected<LuaValue, StarException> luaFrom(T&& t);
  template <typename T>
  [[nodiscard]] std::expected<LuaValue, StarException> luaFrom(T const& t);
  template <typename T>
  [[nodiscard]] std::expected<std::optional<T>, StarException> luaMaybeTo(LuaValue&& v);
  template <typename T>
  [[nodiscard]] std::expected<std::optional<T>, StarException> luaMaybeTo(LuaValue const& v);

  // Wraps luaMaybeTo, reporting a conversion error if the value has no match.
  template <typename T>
  [[nodiscard]] std::expected<T, StarException> luaTo(LuaValue const& v);
  template <typename T>
  [[nodiscard]] std::expected<T, StarException> luaTo(LuaValue&& v);

  [[nodiscard]] std::expected<LuaString, StarException> createString(String const& str);
  [[nodiscard]] std::expected<LuaString, StarException> createString(std::string_view str);
  [[nodiscard]] std::expected<LuaString, StarException> createString(std::string const& str) { return createString(std::string_view(str)); }
  template <std::size_t N>
  [[nodiscard]] std::expected<LuaString, StarException> createString(char const (&str)[N]) { return createString(std::string_view(str)); }

  [[nodiscard]] std::expected<LuaTable, StarException> createTable(int narr = 0, int nrec = 0);

  template <typename Container>
  [[nodiscard]] std::expected<LuaTable, StarException> createTable(Container const& map);

  template <typename Container>
  [[nodiscard]] std::expected<LuaTable, StarException> createArrayTable(Container const& array);

  // Creates a function and deduces the signature of the function using
  // FunctionTraits.  As a convenience, the given function may optionally take
  // a LuaEngine& parameter as the first parameter, and if it does, when called
  // the function will get a reference to the calling LuaEngine.
  template <typename Function>
  [[nodiscard]] std::expected<LuaFunction, StarException> createFunction(Function&& func);

  // If the function signature is not deducible using FunctionTraits, you can
  // specify the return and argument types manually using this createFunction
  // version.
  template <typename Return, typename... Args, typename Function>
  [[nodiscard]] std::expected<LuaFunction, StarException> createFunctionWithSignature(Function&& func);

  [[nodiscard]] std::expected<LuaFunction, StarException> createWrappedFunction(LuaDetail::LuaWrappedFunction function);

  [[nodiscard]] std::expected<LuaFunction, StarException> createRawFunction(lua_CFunction func);

  [[nodiscard]] std::expected<LuaFunction, StarException> createFunctionFromSource(int handleIndex, std::span<char const> contents, String const& name);

  [[nodiscard]] std::expected<LuaThread, StarException> createThread();

  template <typename T>
  [[nodiscard]] std::expected<LuaUserData, StarException> createUserData(T t);

  [[nodiscard]] std::expected<LuaContext, StarException> createContext();

  // Global environment changes only affect newly created contexts

  template <typename T = LuaValue, typename K>
  [[nodiscard]] std::expected<T, StarException> getGlobal(K key);
  template <typename T = LuaValue>
  [[nodiscard]] std::expected<T, StarException> getGlobal(std::string_view key);

  template <typename T, typename K>
  [[nodiscard]] std::expected<void, StarException> setGlobal(K key, T value);

  template <typename T>
  [[nodiscard]] std::expected<void, StarException> setGlobal(std::string_view key, T value);

  // Full collection, or incremental work measured in KiB. Expired native
  // references are also released at ordinary call/resume boundaries.
  [[nodiscard]] std::expected<void, StarException> collectGarbage(std::optional<unsigned> steps = {});

  // Stop / start automatic garbage collection
  void setAutoGarbageCollection(bool autoGarbageColleciton);

  // Tune the pause and step values of the lua garbage collector
  void tuneAutoGarbageCollection(float pause, float stepMultiplier);

  // Bytes in use by lua
  std::size_t memoryUsage() const;

  [[nodiscard]] std::expected<void, StarException> addImGui();
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
  friend std::expected<void, StarException> LuaDetail::insertJsonMetatable(LuaEngine& engine, LuaTable const& table,
                                                                           Json::Type type);

  LuaEngine() = default;

  // Recover the required engine capability from Lua's native extraspace.
  static LuaEngine& luaEngine(lua_State* state);
  // Counts instructions when instruction limiting is enabled.
  static void countHook(lua_State* state, lua_Debug* ar);

  static void* allocate(void* userdata, void* ptr, std::size_t oldSize, std::size_t newSize);

  // Pops a protected Lua error and preserves its diagnostic category.
  [[nodiscard]] std::expected<void, StarException> handleError(lua_State* state, int res);
  [[nodiscard]] std::expected<void, StarException> handleSourceError(lua_State* state, int res, String const& source);
  void pushCallbackError(lua_State* state, StarException const& error);
  std::shared_ptr<ExceptionSummary const> findDiagnostic(lua_State* state, int index) const;
  void rememberDiagnostic(lua_State* state, int index, std::shared_ptr<ExceptionSummary const> summary);
  ExceptionSummary captureDiagnostic(lua_State* from, lua_State* state, int index,
      ExceptionSummaryKind kind) const;
  void beginDiagnosticScope();
  void endDiagnosticScope();

  // lua_pcall with a better message handler that includes a traceback.
  int pcallWithTraceback(lua_State* state, int nargs, int nresults);
  String sourceDiagnostic(String const& source) const noexcept;
  void appendSourceDiagnostics(lua_State* from, lua_State* to, ExceptionSummary const& summary) const noexcept;

  // override for lua coroutine resume with traceback
  static int coresumeWithTraceback(lua_State* state);
  // propagates errors from one state to another, i.e. past thread boundaries
  // pops error off the top of the from stack and pushes onto the to stack
  static void propagateErrorWithTraceback(lua_State* from, lua_State* to);

  std::size_t stringLength(int handleIndex);
  String string(int handleIndex);
  StringView stringView(int handleIndex);


  [[nodiscard]] std::expected<std::optional<LuaTable>, StarException> tableGetMetatable(int handleIndex);
  [[nodiscard]] std::expected<void, StarException> tableSetMetatable(int handleIndex, LuaTable const& table);

  template <typename Ret, typename... Args>
  [[nodiscard]] std::expected<Ret, StarException> callFunction(int handleIndex, Args const&... args);
  template <typename Ret> [[nodiscard]] std::expected<Ret, StarException> callStack(lua_State* state, int nargs);
  template <typename Ret, typename... Args>
  [[nodiscard]] std::expected<std::optional<Ret>, StarException> resumeThread(int handleIndex, Args const&... args);
  [[nodiscard]] std::expected<void, StarException> threadPushFunction(int threadIndex, int functionIndex);
  LuaThread::Status threadStatus(int handleIndex);

  template <typename T>
  [[nodiscard]] std::expected<void, StarException> registerUserDataType();

  template <typename T>
  [[nodiscard]] std::expected<bool, StarException> userDataIsType(int handleIndex);

  template <typename T>
  [[nodiscard]] std::expected<std::reference_wrapper<T>, StarException> getUserData(int handleIndex);

  [[nodiscard]] std::expected<void, StarException> setContextRequire(int handleIndex, LuaContext::RequireFunction requireFunction);

  [[nodiscard]] std::expected<int, StarException> contextEval(int handleIndex, String const& lua, int nresults);
  [[nodiscard]] std::expected<bool, StarException> pushContextPath(int handleIndex, std::string_view path);

  [[nodiscard]] std::expected<LuaValue, StarException> contextGetPath(int handleIndex, String path);
  [[nodiscard]] std::expected<void, StarException> contextSetPath(int handleIndex, String path, LuaValue const& value);

  [[nodiscard]] std::expected<int, StarException> popHandle(lua_State* state);
  void pushHandle(lua_State* state, int handleIndex);
  [[nodiscard]] std::expected<int, StarException> copyHandle(int handleIndex);
  // Unref is allocation-free after the registry freelist has been initialized.
  void releaseHandles(lua_State* state);
  void destroyHandle(int handleIndex);

  [[nodiscard]] std::expected<void, StarException> pushLuaValue(lua_State* state, LuaValue const& luaValue);
  [[nodiscard]] std::expected<LuaValue, StarException> popLuaValue(lua_State* state);
  [[nodiscard]] std::expected<std::optional<Json>, StarException> stackToJson(lua_State* state, int index);
  // Allocates under a protected Lua call, leaving uninitialized storage on top.
  [[nodiscard]] std::expected<void*, StarException> allocateUserData(lua_State* state, std::size_t size);

  template <typename T>
  [[nodiscard]] std::expected<std::size_t, StarException> pushArgument(lua_State* state, T const& arg);

  template <typename T>
  [[nodiscard]] std::expected<std::size_t, StarException> pushArgument(lua_State* state, LuaVariadic<T> const& args);

  [[nodiscard]] std::expected<std::size_t, StarException> doPushArguments(lua_State*);
  template <typename First, typename... Rest>
  [[nodiscard]] std::expected<std::size_t, StarException> doPushArguments(lua_State* state, First const& first, Rest const&... rest);

  template <typename... Args>
  [[nodiscard]] std::expected<std::size_t, StarException> pushArguments(lua_State* state, Args const&... args);

  [[nodiscard]] std::expected<void, StarException> incrementRecursionLevel();
  void decrementRecursionLevel();

  void updateCountHook();

  // The following fields exist to use their addresses as unique lightuserdata,
  // as is recommended by the lua docs.
  static int s_luaInstructionLimitExceptionKey;
  static int s_luaRecursionLimitExceptionKey;

  lua_State* m_state;
  ImageResolver m_imageResolver;
  std::unique_ptr<LuaExecutionHost> m_executionHost;
  SourceResolver m_sourceResolver;
  std::stop_token m_promiseLifetime;
  // Bounded, registry-pinned error-string identities; no Lua reference owns the engine.
  struct DiagnosticTransport {
    void const* identity = nullptr;
    int type = LUA_TNONE;
    std::size_t length = 0;
    std::shared_ptr<ExceptionSummary const> summary;
  };
  std::array<DiagnosticTransport, 64> m_diagnosticTransport;
  std::size_t m_diagnosticCount = 0;
  std::size_t m_nextDiagnostic = 0;
  unsigned m_diagnosticDepth = 0;
  int m_diagnosticStringsRegistryId = LUA_NOREF;
  int m_pcallTracebackMessageHandlerRegistryId;
  int m_scriptDefaultEnvRegistryId;
  int m_wrappedFunctionMetatableRegistryId;
  int m_requireFunctionMetatableRegistryId;
  HashMap<TypeKey, int> m_registeredUserDataTypes;

  List<int> m_handleFree;

  std::uint64_t m_instructionLimit;
  bool m_profilingEnabled;
  unsigned m_instructionMeasureInterval;
  std::uint64_t m_instructionCount;
  unsigned m_recursionLevel;
  unsigned m_recursionLimit;
  std::string m_warningMessage;
  HashMap<std::tuple<String, unsigned>, std::shared_ptr<LuaProfileEntry>> m_profileEntries;
  lua_Debug m_debugInfo;
};

// Built in conversions

template <>
struct LuaConverter<bool> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, bool v) {
    lua_pushboolean(stack.state(), v);
    return {};
  }
  [[nodiscard]] static std::expected<std::optional<bool>, StarException> to(LuaStackValue v) {
    return lua_toboolean(v.stack().state(), v.index()) != 0;
  }
};

template <typename T>
struct LuaIntConverter {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, T v) {
    lua_pushinteger(stack.state(), static_cast<LuaInt>(v));
    return {};
  }
  [[nodiscard]] static std::expected<std::optional<T>, StarException> to(LuaStackValue v) {
    auto fromFloat = [](LuaFloat value) -> std::optional<T> {
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
      return fromFloat(v.as<LuaFloat>());
    if (v.is<LuaString>()) {
      auto text = v.stringView().utf8();
      text = text.substr(0, text.find('\0'));
      if (auto n = maybeLexicalCast<LuaInt>(text))
        return static_cast<T>(*n);
      if (auto n = maybeLexicalCast<LuaFloat>(text))
        return fromFloat(*n);
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
    lua_pushnumber(stack.state(), static_cast<LuaFloat>(v));
    return {};
  }
  [[nodiscard]] static std::expected<std::optional<T>, StarException> to(LuaStackValue v) {
    if (v.is<LuaFloat>())
      return static_cast<T>(v.as<LuaFloat>());
    if (v.is<LuaInt>())
      return static_cast<T>(v.as<LuaInt>());
    if (v.is<LuaString>()) {
      auto text = v.stringView().utf8();
      text = text.substr(0, text.find('\0'));
      if (auto n = maybeLexicalCast<LuaFloat>(text))
        return static_cast<T>(*n);
      if (auto n = maybeLexicalCast<LuaInt>(text))
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
    return stack.pushString(v.utf8());
  }
  [[nodiscard]] static std::expected<std::optional<String>, StarException> to(LuaStackValue v) {
    if (v.is<LuaString>())
      return v.toString();
    if (v.is<LuaInt>())
      return String(toString(v.as<LuaInt>()));
    if (v.is<LuaFloat>())
      return String(toString(v.as<LuaFloat>()));
    return std::nullopt;
  }
};

template <>
struct LuaConverter<std::string> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, std::string const& v) {
    return stack.pushString(v);
  }
  [[nodiscard]] static std::expected<std::optional<std::string>, StarException> to(LuaStackValue v) {
    if (v.is<LuaString>())
      return std::string(v.stringView().utf8());
    auto converted = v.maybeTo<String>();
    if (!converted)
      return std::unexpected(std::move(converted.error()));
    if (!*converted)
      return std::nullopt;
    return std::move(**converted).takeUtf8();
  }
};

template <>
struct LuaConverter<std::string_view> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, std::string_view v) {
    return stack.pushString(v);
  }
};

template <std::size_t s>
struct LuaConverter<char[s]> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, char const (&v)[s]) {
    return stack.pushString(std::string_view(v));
  }
};

template <>
struct LuaConverter<Directives> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, Directives const& v) {
    auto string = v.maybeString();
    return stack.pushString(string ? string->get().utf8() : std::string_view());
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
      return v.stack().engine().createString(toString(v.as<LuaInt>()));
    if (v.is<LuaFloat>())
      return v.stack().engine().createString(toString(v.as<LuaFloat>()));
    return std::nullopt;
  }
};

template <>
struct LuaConverter<LuaTable> : LuaValueConverter<LuaTable> {};
template <>
struct LuaConverter<LuaFunction> : LuaValueConverter<LuaFunction> {};
template <>
struct LuaConverter<LuaThread> : LuaValueConverter<LuaThread> {};
template <>
struct LuaConverter<LuaUserData> : LuaValueConverter<LuaUserData> {};

template <>
struct LuaConverter<LuaValue> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, LuaValue const& v) {
    return stack.engine().pushLuaValue(stack.state(), v);
  }
  [[nodiscard]] static std::expected<LuaValue, StarException> to(LuaStackValue v) { return v.own(); }
};

template <> struct LuaConverter<LuaStackValue> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, LuaStackValue v) {
    if (&stack.engine() != &v.stack().engine())
      return std::unexpected(LuaException("Lua stack values cannot be shared between engines"));
    if (stack.state() != v.stack().state() && !lua_checkstack(v.stack().state(), 1))
      return std::unexpected(LuaException("Exhausted the Lua stack while copying a borrowed value"));
    lua_pushvalue(v.stack().state(), v.index());
    if (stack.state() != v.stack().state())
      lua_xmove(v.stack().state(), stack.state(), 1);
    return {};
  }
};

template <typename T>
struct LuaConverter<std::optional<T>> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, std::optional<T> const& v) {
    return v ? stack.push(*v) : stack.push(LuaNil);
  }
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, std::optional<T>&& v) {
    if (!v)
      return stack.push(LuaNil);
    T value = std::move(*v);
    v.reset();
    return stack.push(std::move(value));
  }
  [[nodiscard]] static std::expected<std::optional<std::optional<T>>, StarException> to(LuaStackValue v) {
    if (v.is<LuaNilType>())
      return std::optional<std::optional<T>>(std::in_place);
    auto converted = v.maybeTo<T>();
    if (!converted)
      return std::unexpected(std::move(converted.error()));
    if (!*converted)
      return std::nullopt;
    return std::optional<std::optional<T>>(std::in_place, std::move(*converted));
  }
};

template <typename T>
struct LuaMapConverter {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, T const& v) { return stack.pushMap(v); }
  [[nodiscard]] static std::expected<std::optional<T>, StarException> to(LuaStackValue v) {
    if (!v.is<LuaTable>())
      return std::nullopt;
    T result;
    bool mismatch = false;
    auto iterated = v.iterate([&](LuaStackValue key, LuaStackValue value) -> std::expected<bool, StarException> {
      auto contKey = key.maybeTo<typename T::key_type>();
      if (!contKey)
        return std::unexpected(std::move(contKey.error()));
      auto contValue = value.maybeTo<typename T::mapped_type>();
      if (!contValue)
        return std::unexpected(std::move(contValue.error()));
      if (!*contKey || !*contValue) {
        mismatch = true;
        return false;
      }
      result.set(std::move(**contKey), std::move(**contValue));
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
    return stack.pushArray(v);
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
      auto converted = value.maybeTo<typename T::value_type>();
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

template <typename T, std::size_t MaxSize>
struct LuaConverter<StaticList<T, MaxSize>> : LuaContainerConverter<StaticList<T, MaxSize>> {};

template <typename T, std::size_t MaxStackSize>
struct LuaConverter<SmallList<T, MaxStackSize>> : LuaContainerConverter<SmallList<T, MaxStackSize>> {};

template <>
struct LuaConverter<StringList> : LuaContainerConverter<StringList> {};

template <SetContainer Container>
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
  inline LuaRegistryReference::LuaRegistryReference(LuaEnginePtr engine, int handleIndex)
    : engine(std::move(engine)), handleIndex(handleIndex) {}

  inline LuaRegistryReference::~LuaRegistryReference() {
    if (engine)
      engine->destroyHandle(handleIndex);
  }

  inline LuaHandle::LuaHandle(LuaEnginePtr engine, int handleIndex)
    : reference(std::make_shared<LuaRegistryReference>(std::move(engine), handleIndex)) {}

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
    static constexpr int Count = 1;
    [[nodiscard]] static std::expected<T, StarException> read(LuaStack stack, int first, int) {
      return stack.read<T>(first);
    }
  };
  template <> struct LuaReturn<void> {
    static constexpr int Count = 0;
    [[nodiscard]] static std::expected<void, StarException> read(LuaStack, int, int) { return {}; }
  };
  template <typename T> struct LuaReturn<LuaVariadic<T>> {
    static constexpr int Count = LUA_MULTRET;
    [[nodiscard]] static std::expected<LuaVariadic<T>, StarException> read(LuaStack stack, int first, int count) {
      LuaVariadic<T> result;
      result.reserve(count);
      for (int i = 0; i < count; ++i) {
        auto value = stack.read<T>(first + i);
        if (!value)
          return std::unexpected(std::move(value.error()));
        result.append(std::move(*value));
      }
      return result;
    }
  };
  template <typename... Types> struct LuaReturn<LuaTupleReturn<Types...>> {
    static constexpr int Count = sizeof...(Types);
    [[nodiscard]] static std::expected<LuaTupleReturn<Types...>, StarException> read(LuaStack stack, int first,
                                                                                     int count) {
      auto getter = [&]<typename T>(std::size_t index) {
        return stack.read<T>(index < static_cast<std::size_t>(count) ? first + static_cast<int>(index)
                                                                     : stack.top() + 1);
      };
      return ConvertedPack<Types...>::apply(getter, 0,
          [](Types... values) -> std::expected<LuaTupleReturn<Types...>, StarException> {
            return LuaTupleReturn<Types...>(std::move(values)...);
          });
    }
  };

  template <typename T>
  [[nodiscard]] std::expected<int, StarException> toWrappedReturn(LuaStack stack, T const& value) {
    auto pushed = stack.push(value);
    if (!pushed)
      return std::unexpected(std::move(pushed.error()));
    return 1;
  }
  template <typename... Types>
  [[nodiscard]] std::expected<int, StarException> toWrappedReturn(LuaStack stack,
                                                                  LuaTupleReturn<Types...> const& values) {
    std::expected<void, StarException> status;
    auto append = [&](auto const& value) {
      if (status)
        status = stack.push(value);
    };
    std::apply([&](auto const&... value) { (append(value), ...); }, static_cast<typename LuaTupleReturn<Types...>::Base const&>(values));
    if (!status)
      return std::unexpected(std::move(status.error()));
    return static_cast<int>(sizeof...(Types));
  }
  template <typename T>
  [[nodiscard]] std::expected<int, StarException> toWrappedReturn(LuaStack stack, LuaVariadic<T> const& values) {
    if (values.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
      return std::unexpected(LuaException("Too many Lua return values"));
    for (auto const& value : values) {
      auto pushed = stack.push(value);
      if (!pushed)
        return std::unexpected(std::move(pushed.error()));
    }
    return static_cast<int>(values.size());
  }
  template <typename Return, typename Function, typename... Args>
  [[nodiscard]] std::expected<int, StarException> invokeWrapped(LuaStack stack, Function&& function, Args&&... args) {
    using DeclaredReturn = typename AlgorithmResultInfo<std::remove_cvref_t<Return>>::Value;
    using ActualReturn = std::remove_cvref_t<std::invoke_result_t<Function, Args...>>;
    using ActualResultInfo = AlgorithmResultInfo<ActualReturn>;
    if constexpr (std::is_void_v<ActualReturn>) {
      static_assert(std::is_void_v<DeclaredReturn>);
      std::invoke(std::forward<Function>(function), std::forward<Args>(args)...);
      return toWrappedReturn(stack, LuaNil);
    } else {
      auto&& result = std::invoke(std::forward<Function>(function), std::forward<Args>(args)...);
      if constexpr (ActualResultInfo::Fallible) {
        if (!result)
          return std::unexpected(std::move(result.error()));
        if constexpr (std::is_void_v<DeclaredReturn>) {
          return toWrappedReturn(stack, LuaNil);
        } else if constexpr (std::is_same_v<DeclaredReturn, typename ActualResultInfo::Value>) {
          return toWrappedReturn(stack, *result);
        } else {
          return toWrappedReturn(stack, static_cast<DeclaredReturn>(std::move(*result)));
        }
      } else if constexpr (std::is_void_v<DeclaredReturn>) {
        return toWrappedReturn(stack, LuaNil);
      } else if constexpr (std::is_same_v<DeclaredReturn, ActualReturn>) {
        return toWrappedReturn(stack, result);
      } else {
        return toWrappedReturn(stack, static_cast<DeclaredReturn>(std::forward<decltype(result)>(result)));
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
          getter, 0, [&](Args&&... args) { return invokeWrapped<Return>(stack, function, std::move(args)...); });
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
          return invokeWrapped<Return>(stack, function, stack.engine(), std::move(args)...);
        });
      };
    }
  };
  template <typename Return, typename... Args, typename Function>
  LuaWrappedFunction wrapFunctionWithSignature(Function&& function) {
    return FunctionWrapper<Return, std::decay_t<Args>...>::wrap(std::forward<Function>(function));
  }
  template <typename Return, typename Function, typename... Args>
  LuaWrappedFunction wrapFunctionArgs(Function&& function, std::type_identity<std::tuple<Args...>>) {
    return wrapFunctionWithSignature<Return, Args...>(std::forward<Function>(function));
  }
  template <typename Function>
  LuaWrappedFunction wrapFunction(Function&& function) {
    return wrapFunctionArgs<typename FunctionTraits<Function>::Return>(
      std::forward<Function>(function), std::type_identity<typename FunctionTraits<Function>::ArgTuple>{});
  }

  template <typename Return, typename T, typename... Args>
  struct MethodWrapper {
    template <typename Function>
    static LuaWrappedFunction wrap(Function function) {
      return [function = std::move(function)](LuaStack stack) mutable -> std::expected<int, StarException> {
        if (stack.top() == 0)
          return std::unexpected(LuaException("No object argument passed to wrapped method"));
        auto object = stack.value(1).userData<T>();
        if (!object)
          return std::unexpected(std::move(object.error()));
        auto getter = [&]<typename A>(std::size_t index) { return ArgGet<A>::get(stack, index + 1); };
        return ConvertedPack<Args...>::apply(getter, 0, [&](Args&&... args) {
          return invokeWrapped<Return>(stack, function, object->get(), std::move(args)...);
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
        auto object = stack.value(1).userData<T>();
        if (!object)
          return std::unexpected(std::move(object.error()));
        auto getter = [&]<typename A>(std::size_t index) { return ArgGet<A>::get(stack, index + 1); };
        return ConvertedPack<Args...>::apply(getter, 0, [&](Args&&... args) {
          return invokeWrapped<Return>(stack, function, object->get(), stack.engine(), std::move(args)...);
        });
      };
    }
  };
  template <typename Return, typename... Args, typename Function>
  LuaWrappedFunction wrapMethodWithSignature(Function&& function) {
    return MethodWrapper<Return, std::decay_t<Args>...>::wrap(std::forward<Function>(function));
  }
  template <typename Return, typename Function, typename... Args>
  LuaWrappedFunction wrapMethodArgs(Function&& function, std::type_identity<std::tuple<Args...>>) {
    return wrapMethodWithSignature<Return, Args...>(std::forward<Function>(function));
  }
  template <typename Function>
  LuaWrappedFunction wrapMethod(Function&& function) {
    return wrapMethodArgs<typename FunctionTraits<Function>::Return>(
      std::forward<Function>(function), std::type_identity<typename FunctionTraits<Function>::ArgTuple>{});
  }

  template <typename Function, typename... Args>
  [[nodiscard]] std::expected<bool, StarException> invokeIterator(Function& function, Args&&... args) {
    using Return = std::invoke_result_t<Function&, Args...>;
    if constexpr (std::is_void_v<Return>) {
      std::invoke(function, std::forward<Args>(args)...);
      return true;
    } else if constexpr (AlgorithmResultInfo<Return>::Fallible) {
      auto result = std::invoke(function, std::forward<Args>(args)...);
      if (!result)
        return std::unexpected(std::move(result.error()));
      if constexpr (std::is_void_v<typename AlgorithmResultInfo<Return>::Value>)
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
        auto convertedKey = key.template to<Key>();
        if (!convertedKey)
          return std::unexpected(std::move(convertedKey.error()));
        auto convertedValue = value.template to<Value>();
        if (!convertedValue)
          return std::unexpected(std::move(convertedValue.error()));
        return invokeIterator(function, std::move(*convertedKey), std::move(*convertedValue));
      };
    }
  };
  template <typename Ret, typename Key, typename Value>
  struct TableIteratorWrapper<Ret, LuaEngine, Key, Value> {
    template <typename Function> static auto wrap(LuaEngine& engine, Function&& function) {
      return [&engine, function = std::forward<Function>(function)](
               LuaStackValue key, LuaStackValue value) mutable -> std::expected<bool, StarException> {
        auto convertedKey = key.template to<Key>();
        if (!convertedKey)
          return std::unexpected(std::move(convertedKey.error()));
        auto convertedValue = value.template to<Value>();
        if (!convertedValue)
          return std::unexpected(std::move(convertedValue.error()));
        return invokeIterator(function, engine, std::move(*convertedKey), std::move(*convertedValue));
      };
    }
  };
  template <typename Return, typename... Args, typename Function>
  auto wrapTableIteratorWithSignature(LuaEngine& engine, Function&& function) {
    return TableIteratorWrapper<Return, std::decay_t<Args>...>::wrap(engine, std::forward<Function>(function));
  }
  template <typename Return, typename Function, typename... Args>
  auto wrapTableIteratorArgs(LuaEngine& engine, Function&& function, std::type_identity<std::tuple<Args...>>) {
    return wrapTableIteratorWithSignature<Return, Args...>(engine, std::forward<Function>(function));
  }
  template <typename Function> auto wrapTableIterator(LuaEngine& engine, Function&& function) {
    return wrapTableIteratorArgs<typename FunctionTraits<Function>::Return>(
      engine, std::forward<Function>(function), std::type_identity<typename FunctionTraits<Function>::ArgTuple>{});
  }

  // Like lua_setfield but raw; used inside protected Lua calls.
  void rawSetField(lua_State* state, int index, std::string_view key);

  // Shallow copies a lua table at the given index into the table at the target
  // index.
  void shallowCopy(lua_State* state, int sourceIndex, int targetIndex);


  // Creates a custom lua table from a JsonArray or JsonObject that has
  // slightly different behavior than a standard lua table.  The table
  // remembers nil entries, as well as whether it was initially constructed
  // from a JsonArray or JsonObject as a hint on how to convert it back into a
  // Json.  The custom containers are meant to act nearly identical to standard
  // lua tables, so iterating over the table with pairs or ipairs works exactly
  // like a standard lua table, so will skip over nil entries and in the case
  // of ipairs, stop at the first nil entry.
  [[nodiscard]] std::expected<LuaTable, StarException> jsonContainerToTable(LuaEngine& engine, Json const& container);


  // Special lua functions to operate on our custom jarray / jobject container
  // types.  Should always do some "sensible" action if given a regular lua
  // table instead of a custom json container one.

  // Create a JsonList container table
  Json jarrayCreate();
  // Create a JsonMap container table
  Json jobjectCreate();

  // Adds the Json array metatable to a Lua table or creates one.
  [[nodiscard]] std::expected<LuaTable, StarException> jarray(LuaEngine& engine, std::optional<LuaTable> table);
  // Adds the Json object metatable to a Lua table or creates one.
  [[nodiscard]] std::expected<LuaTable, StarException> jobject(LuaEngine& engine, std::optional<LuaTable> table);

  // *Really* remove an entry from a JsonList or JsonMap container table,
  // including removing it from the __nils table.  If the given table is not a
  // special container table, is equivalent to setting the key entry to nil.
  [[nodiscard]] std::expected<void, StarException> jcontRemove(LuaTable const& t, LuaValue const& key);

  // Returns the element count of the lua table argument, or, in the case of a
  // special JsonList container table, returns the "true" element count
  // including any nil entries.
  [[nodiscard]] std::expected<std::size_t, StarException> jcontSize(LuaTable const& t);

  // Resize the given lua table by removing any indexed entries greater than the
  // target size, and in the case of a special JsonList container table, pads
  // to the end of the new size with nil entries.
  [[nodiscard]] std::expected<void, StarException> jcontResize(LuaTable const& t, std::size_t size);

  // Coerces a values (strings, floats, ints) into an integer, but fails if the
  // number looks fractional (does not parse as int, float is not an exact
  // integer)
  std::optional<LuaInt> asInteger(LuaValue const& v);
}

template <typename Container>
LuaVariadic<typename std::decay<Container>::type::value_type> luaUnpack(Container&& c) {
  LuaVariadic<typename std::decay<Container>::type::value_type> ret;
  if (std::is_rvalue_reference<Container&&>::value) {
    for (auto& e : c)
      ret.append(std::move(e));
  } else {
    for (auto const& e : c)
      ret.append(e);
  }
  return ret;
}

template <typename... Types>
LuaTupleReturn<Types...>::LuaTupleReturn(Types const&... args)
  : Base(args...) {}

template <typename... Types>
template <typename... UTypes>
LuaTupleReturn<Types...>::LuaTupleReturn(UTypes&&... args)
  : Base(std::move(args)...) {}

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
LuaTupleReturn<Types&...> luaTie(Types&... args) {
  return LuaTupleReturn<Types&...>(args...);
}

template <typename... Types>
LuaTupleReturn<typename std::decay<Types>::type...> luaTupleReturn(Types&&... args) {
  return LuaTupleReturn<typename std::decay<Types>::type...>(std::forward<Types>(args)...);
}

namespace LuaDetail {
  struct LuaStackRestore {
    lua_State* state;
    int top;

    explicit LuaStackRestore(lua_State* state) : state(state), top(lua_gettop(state)) {}
    LuaStackRestore(LuaStackRestore const&) = delete;
    LuaStackRestore& operator=(LuaStackRestore const&) = delete;
    ~LuaStackRestore() { lua_settop(state, top); }
  };

  template <typename T>
  inline constexpr bool LuaValueAlternative = []<typename... Types>(std::variant<Types...>*) {
    return (std::is_same_v<std::remove_cvref_t<T>, Types> || ...);
  }(static_cast<LuaValue*>(nullptr));

  template <typename T, typename Result>
  [[nodiscard]] std::expected<std::optional<T>, StarException> normalizeMaybeTo(Result&& result) {
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
    if (!lua_checkstack(m_state, 4))
      return std::unexpected(LuaException("Exhausted the Lua stack while pushing a value"));
    auto pushed = [&]() -> std::expected<void, StarException> {
      if constexpr ((std::is_pointer_v<std::remove_reference_t<T>> || std::is_array_v<std::remove_reference_t<T>>)
                    && std::is_convertible_v<T, std::string_view>) {
        if constexpr (std::is_pointer_v<std::remove_reference_t<T>>) {
          if (!value)
            return std::unexpected(LuaException("Cannot create a Lua string from a null pointer"));
        }
        return pushString(std::string_view(value));
      } else {
        return LuaConverter<std::decay_t<T>>::from(*this, std::forward<T>(value));
      }
    }();
    if (pushed)
      ++restore.top;
    return pushed;
  }

  template <typename T> [[nodiscard]] std::expected<T, StarException> LuaStack::read(int index) const {
    return value(index).to<T>();
  }
  template <typename T>
  [[nodiscard]] std::expected<std::optional<T>, StarException> LuaStack::maybeRead(int index) const {
    return value(index).maybeTo<T>();
  }

  template <typename T> bool LuaStackValue::is() const {
    auto state = m_stack.state();
    if constexpr (std::is_same_v<T, LuaNilType>)
      return lua_isnoneornil(state, m_index);
    else if constexpr (std::is_same_v<T, LuaBoolean>)
      return lua_type(state, m_index) == LUA_TBOOLEAN;
    else if constexpr (std::is_same_v<T, LuaInt>)
      return lua_isinteger(state, m_index);
    else if constexpr (std::is_same_v<T, LuaFloat>)
      return lua_type(state, m_index) == LUA_TNUMBER && !lua_isinteger(state, m_index);
    else if constexpr (std::is_same_v<T, LuaString>)
      return lua_type(state, m_index) == LUA_TSTRING;
    else if constexpr (std::is_same_v<T, LuaTable>)
      return lua_type(state, m_index) == LUA_TTABLE;
    else if constexpr (std::is_same_v<T, LuaFunction>)
      return lua_type(state, m_index) == LUA_TFUNCTION;
    else if constexpr (std::is_same_v<T, LuaThread>)
      return lua_type(state, m_index) == LUA_TTHREAD;
    else {
      static_assert(std::is_same_v<T, LuaUserData>);
      return lua_type(state, m_index) == LUA_TUSERDATA;
    }
  }
  template <typename T> T LuaStackValue::as() const {
    if constexpr (std::is_same_v<T, LuaBoolean>)
      return lua_toboolean(m_stack.state(), m_index) != 0;
    else if constexpr (std::is_same_v<T, LuaInt>)
      return lua_tointeger(m_stack.state(), m_index);
    else {
      static_assert(std::is_same_v<T, LuaFloat>);
      return lua_tonumber(m_stack.state(), m_index);
    }
  }
  template <typename T> [[nodiscard]] std::expected<std::optional<T>, StarException> LuaStackValue::maybeTo() const {
    if (auto valid = validate(); !valid)
      return std::unexpected(std::move(valid.error()));
    return LuaDetail::normalizeMaybeTo<T>(LuaConverter<T>::to(*this));
  }
  template <typename T> [[nodiscard]] std::expected<T, StarException> LuaStackValue::to() const {
    auto converted = maybeTo<T>();
    if (!converted)
      return std::unexpected(std::move(converted.error()));
    if (!*converted)
      return std::unexpected(LuaConversionException::format("Error converting LuaValue to type '{}'", typeName<T>()));
    return std::move(**converted);
  }

  template <typename K>
  [[nodiscard]] std::expected<void, StarException> LuaStackValue::pushField(K const& key, bool raw) const {
    auto state = m_stack.state();
    if (!lua_checkstack(state, 5))
      return std::unexpected(LuaException("Exhausted the Lua stack while reading a table"));
    lua_pushvalue(state, m_index);
    if (auto pushed = m_stack.push(key); !pushed)
      return std::unexpected(std::move(pushed.error()));
    return m_stack.call(raw ? +[](lua_State* s) { lua_rawget(s, 1); return 1; }
                         : +[](lua_State* s) { lua_gettable(s, 1); return 1; }, 2, 1);
  }
  template <typename T, typename K>
  [[nodiscard]] std::expected<T, StarException> LuaStackValue::get(K const& key) const {
    LuaDetail::LuaStackRestore restore(m_stack.state());
    if (auto pushed = pushField(key, false); !pushed)
      return std::unexpected(std::move(pushed.error()));
    return m_stack.read<T>(-1);
  }
  template <typename T, typename K>
  [[nodiscard]] std::expected<std::optional<T>, StarException> LuaStackValue::maybeGet(K const& key) const {
    LuaDetail::LuaStackRestore restore(m_stack.state());
    if (auto pushed = pushField(key, false); !pushed)
      return std::unexpected(std::move(pushed.error()));
    return m_stack.maybeRead<T>(-1);
  }
  template <typename K> [[nodiscard]] std::expected<bool, StarException> LuaStackValue::contains(K const& key) const {
    LuaDetail::LuaStackRestore restore(m_stack.state());
    if (auto pushed = pushField(key, false); !pushed)
      return std::unexpected(std::move(pushed.error()));
    if (auto valid = m_stack.value(-1).validate(); !valid)
      return std::unexpected(std::move(valid.error()));
    return !m_stack.value(-1).is<LuaNilType>();
  }
  template <typename T, typename K>
  [[nodiscard]] std::expected<T, StarException> LuaStackValue::rawGet(K const& key) const {
    LuaDetail::LuaStackRestore restore(m_stack.state());
    if (auto pushed = pushField(key, true); !pushed)
      return std::unexpected(std::move(pushed.error()));
    return m_stack.read<T>(-1);
  }
  template <typename K, typename V>
  [[nodiscard]] std::expected<void, StarException> LuaStackValue::setField(K const& key, V&& value, bool raw) const {
    auto state = m_stack.state();
    LuaDetail::LuaStackRestore restore(state);
    if (!lua_checkstack(state, 6))
      return std::unexpected(LuaException("Exhausted the Lua stack while writing a table"));
    lua_pushvalue(state, m_index);
    if (auto pushed = m_stack.push(key); !pushed)
      return std::unexpected(std::move(pushed.error()));
    if (auto pushed = m_stack.push(std::forward<V>(value)); !pushed)
      return std::unexpected(std::move(pushed.error()));
    return m_stack.call(raw ? +[](lua_State* s) { lua_rawset(s, 1); return 0; }
                         : +[](lua_State* s) { lua_settable(s, 1); return 0; }, 3, 0);
  }
  template <typename K, typename V>
  [[nodiscard]] std::expected<void, StarException> LuaStackValue::set(K const& key, V&& value) const {
    return setField(key, std::forward<V>(value), false);
  }
  template <typename K, typename V>
  [[nodiscard]] std::expected<void, StarException> LuaStackValue::rawSet(K const& key, V&& value) const {
    return setField(key, std::forward<V>(value), true);
  }
  template <typename Function>
  [[nodiscard]] std::expected<void, StarException> LuaStackValue::iterate(Function&& function) const {
    auto state = m_stack.state();
    LuaDetail::LuaStackRestore restore(state);
    if (!lua_checkstack(state, 6))
      return std::unexpected(LuaException("Exhausted the Lua stack while iterating a table"));
    lua_pushnil(state);
    int cursor = lua_gettop(state);
    while (true) {
      lua_pushvalue(state, m_index);
      lua_pushvalue(state, cursor);
      auto next = m_stack.call(+[](lua_State* s) { return lua_next(s, 1) ? 2 : 0; }, 2, LUA_MULTRET);
      if (!next)
        return std::unexpected(std::move(next.error()));
      lua_remove(state, cursor);
      if (lua_gettop(state) < cursor)
        return {};
      auto key = m_stack.value(cursor);
      auto value = m_stack.value(cursor + 1);
      if (auto valid = key.validate(); !valid)
        return std::unexpected(std::move(valid.error()));
      if (auto valid = value.validate(); !valid)
        return std::unexpected(std::move(valid.error()));
      auto cont = LuaDetail::invokeIterator(function, key, value);
      if (!cont)
        return std::unexpected(std::move(cont.error()));
      if (!*cont)
        return {};
      lua_pop(state, 1);
    }
  }

  template <typename Container>
  [[nodiscard]] std::expected<void, StarException> LuaStack::pushArray(Container const& values) const {
    if (values.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
      return std::unexpected(LuaException("Too many entries for a Lua array table"));
    auto table = createTable(static_cast<int>(values.size()), 0);
    if (!table)
      return std::unexpected(std::move(table.error()));
    LuaInt index = 0;
    for (auto const& value : values) {
      if (auto set = table->rawSet(++index, value); !set)
        return std::unexpected(std::move(set.error()));
    }
    return {};
  }
  template <typename Container>
  [[nodiscard]] std::expected<void, StarException> LuaStack::pushMap(Container const& values) const {
    if (values.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
      return std::unexpected(LuaException("Too many entries for a Lua table"));
    auto table = createTable(0, static_cast<int>(values.size()));
    if (!table)
      return std::unexpected(std::move(table.error()));
    for (auto const& pair : values) {
      if (auto set = table->rawSet(pair.first, pair.second); !set)
        return std::unexpected(std::move(set.error()));
    }
    return {};
  }

  template <typename T> [[nodiscard]] std::expected<void, StarException> LuaStack::pushUserData(T value) const {
    if (auto registered = m_engine.registerUserDataType<T>(); !registered)
      return std::unexpected(std::move(registered.error()));
    if (!lua_checkstack(m_state, 2))
      return std::unexpected(LuaException("Exhausted the Lua stack while creating userdata"));
    auto storage = m_engine.allocateUserData(m_state, sizeof(T));
    if (!storage)
      return std::unexpected(std::move(storage.error()));
    new (*storage) T(std::move(value));
    lua_rawgeti(m_state, LUA_REGISTRYINDEX, m_engine.m_registeredUserDataTypes.value(typeKey<T>(), LUA_NOREF));
    lua_setmetatable(m_state, -2);
    return {};
  }
  template <typename T> [[nodiscard]] std::expected<bool, StarException> LuaStackValue::isUserData() const {
    if (!is<LuaUserData>())
      return false;
    auto& engine = m_stack.engine();
    int reference = engine.m_registeredUserDataTypes.value(typeKey<T>(), LUA_NOREF);
    if (reference == LUA_NOREF)
      return false;
    auto state = m_stack.state();
    LuaDetail::LuaStackRestore restore(state);
    if (!lua_checkstack(state, 2))
      return std::unexpected(LuaException("Exhausted the Lua stack while checking userdata"));
    if (!lua_getmetatable(state, m_index))
      return std::unexpected(LuaException("Userdata missing metatable in userDataIsType"));
    lua_rawgeti(state, LUA_REGISTRYINDEX, reference);
    return lua_rawequal(state, -1, -2) != 0;
  }
  template <typename T>
  [[nodiscard]] std::expected<std::reference_wrapper<T>, StarException> LuaStackValue::userData() const {
    auto matches = isUserData<T>();
    if (!matches)
      return std::unexpected(std::move(matches.error()));
    if (!*matches)
      return std::unexpected(
        LuaConversionException::format("Improper conversion from userdata to type {}", typeName<T>()));
    return std::ref(*static_cast<T*>(lua_touserdata(m_stack.state(), m_index)));
  }
inline LuaReference::LuaReference(LuaDetail::LuaHandle handle) : m_handle(std::move(handle)) {}

inline bool LuaReference::operator==(LuaReference const& rhs) const {
  if (!m_handle.reference || !rhs.m_handle.reference)
    return m_handle.reference == rhs.m_handle.reference;
  return std::tie(m_handle.reference->engine, m_handle.reference->handleIndex)
      == std::tie(rhs.m_handle.reference->engine, rhs.m_handle.reference->handleIndex);
}

inline LuaEngine& LuaReference::engine() const {
  return *m_handle.reference->engine;
}

inline int LuaReference::handleIndex() const {
  return m_handle.reference ? m_handle.reference->handleIndex : 0;
}


inline std::size_t LuaString::length() const {
  return engine().stringLength(handleIndex());
}

inline String LuaString::toString() const {
  return engine().string(handleIndex());
}

inline StringView LuaString::view() const {
  return engine().stringView(handleIndex());
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
[[nodiscard]] std::expected<T, StarException> LuaTable::get(K key) const {
  auto& eng = engine();
  LuaDetail::LuaStackRestore restore(eng.m_state);
  if (!lua_checkstack(eng.m_state, 1))
    return std::unexpected(LuaException("Exhausted the Lua stack while accessing a table"));
  eng.pushHandle(eng.m_state, handleIndex());
  return LuaStack(eng, eng.m_state).value(-1).template get<T>(key);
}
template <typename T>
[[nodiscard]] std::expected<T, StarException> LuaTable::get(std::string_view key) const {
  return get<T, std::string_view>(key);
}
template <typename T, typename K>
[[nodiscard]] std::expected<void, StarException> LuaTable::set(K key, T value) const {
  auto& eng = engine();
  LuaDetail::LuaStackRestore restore(eng.m_state);
  if (!lua_checkstack(eng.m_state, 1))
    return std::unexpected(LuaException("Exhausted the Lua stack while accessing a table"));
  eng.pushHandle(eng.m_state, handleIndex());
  return LuaStack(eng, eng.m_state).value(-1).set(key, std::move(value));
}
template <typename T>
[[nodiscard]] std::expected<void, StarException> LuaTable::set(std::string_view key, T value) const {
  return set<T, std::string_view>(key, std::move(value));
}
template <typename K>
[[nodiscard]] std::expected<bool, StarException> LuaTable::contains(K key) const {
  auto& eng = engine();
  LuaDetail::LuaStackRestore restore(eng.m_state);
  if (!lua_checkstack(eng.m_state, 1))
    return std::unexpected(LuaException("Exhausted the Lua stack while accessing a table"));
  eng.pushHandle(eng.m_state, handleIndex());
  return LuaStack(eng, eng.m_state).value(-1).contains(key);
}
template <typename K>
[[nodiscard]] std::expected<void, StarException> LuaTable::remove(K key) const {
  return set(std::move(key), LuaNil);
}
template <typename Function>
[[nodiscard]] std::expected<void, StarException> LuaTable::iterate(Function&& function) const {
  auto& eng = engine();
  LuaDetail::LuaStackRestore restore(eng.m_state);
  if (!lua_checkstack(eng.m_state, 1))
    return std::unexpected(LuaException("Exhausted the Lua stack while accessing a table"));
  eng.pushHandle(eng.m_state, handleIndex());
  return LuaStack(eng, eng.m_state)
    .value(-1)
    .iterate(LuaDetail::wrapTableIterator(eng, std::forward<Function>(function)));
}
template <typename Return, typename... Args, typename Function>
[[nodiscard]] std::expected<void, StarException> LuaTable::iterateWithSignature(Function&& function) const {
  auto& eng = engine();
  LuaDetail::LuaStackRestore restore(eng.m_state);
  if (!lua_checkstack(eng.m_state, 1))
    return std::unexpected(LuaException("Exhausted the Lua stack while accessing a table"));
  eng.pushHandle(eng.m_state, handleIndex());
  return LuaStack(eng, eng.m_state)
    .value(-1)
    .iterate(LuaDetail::wrapTableIteratorWithSignature<Return, Args...>(eng, std::forward<Function>(function)));
}
template <typename T, typename K>
[[nodiscard]] std::expected<T, StarException> LuaTable::rawGet(K key) const {
  auto& eng = engine();
  LuaDetail::LuaStackRestore restore(eng.m_state);
  if (!lua_checkstack(eng.m_state, 1))
    return std::unexpected(LuaException("Exhausted the Lua stack while accessing a table"));
  eng.pushHandle(eng.m_state, handleIndex());
  return LuaStack(eng, eng.m_state).value(-1).template rawGet<T>(key);
}
template <typename T>
[[nodiscard]] std::expected<T, StarException> LuaTable::rawGet(std::string_view key) const {
  return rawGet<T, std::string_view>(key);
}
template <typename T, typename K>
[[nodiscard]] std::expected<void, StarException> LuaTable::rawSet(K key, T value) const {
  auto& eng = engine();
  LuaDetail::LuaStackRestore restore(eng.m_state);
  if (!lua_checkstack(eng.m_state, 1))
    return std::unexpected(LuaException("Exhausted the Lua stack while accessing a table"));
  eng.pushHandle(eng.m_state, handleIndex());
  return LuaStack(eng, eng.m_state).value(-1).rawSet(key, std::move(value));
}
template <typename T>
[[nodiscard]] std::expected<void, StarException> LuaTable::rawSet(std::string_view key, T value) const {
  return rawSet<T, std::string_view>(key, std::move(value));
}

template <typename Ret, typename... Args>
[[nodiscard]] std::expected<Ret, StarException> LuaFunction::invoke(Args const&... args) const {
  return engine().template callFunction<Ret>(handleIndex(), args...);
}

template <typename Ret, typename... Args>
[[nodiscard]] std::expected<std::optional<Ret>, StarException> LuaThread::resume(Args const&... args) const {
  return engine().template resumeThread<Ret>(handleIndex(), args...);
}

[[nodiscard]] inline std::expected<void, StarException> LuaThread::pushFunction(LuaFunction const& func) const {
  if (&engine() != &func.engine())
    return std::unexpected(LuaException("Cannot push a function from another Lua engine"));
  return engine().threadPushFunction(handleIndex(), func.handleIndex());
}

inline LuaThread::Status LuaThread::status() const {
  return engine().threadStatus(handleIndex());
}

template <typename T>
[[nodiscard]] std::expected<bool, StarException> LuaUserData::is() const {
  return engine().userDataIsType<T>(handleIndex());
}

template <typename T>
[[nodiscard]] std::expected<std::reference_wrapper<T>, StarException> LuaUserData::get() const {
  return engine().getUserData<T>(handleIndex());
}

template <typename Function>
[[nodiscard]] std::expected<void, StarException> LuaCallbacks::registerCallback(String name, Function&& func) {
  if (!m_callbacks.insert(name, LuaDetail::wrapFunction(std::forward<Function>(func))).second)
    return std::unexpected(LuaException::format("Lua callback '{}' was registered twice", name));
  return {};
}

template <typename Return, typename... Args, typename Function>
[[nodiscard]] std::expected<void, StarException> LuaCallbacks::registerCallbackWithSignature(String name, Function&& func) {
  if (!m_callbacks.insert(name, LuaDetail::wrapFunctionWithSignature<Return, Args...>(std::forward<Function>(func))).second)
    return std::unexpected(LuaException::format("Lua callback '{}' was registered twice", name));
  return {};
}

template <typename T>
template <typename Function>
[[nodiscard]] std::expected<void, StarException> LuaMethods<T>::registerMethod(String name, Function&& func) {
  if (!m_methods.insert(name, LuaDetail::wrapMethod(std::forward<Function>(func))).second)
    return std::unexpected(LuaException::format("Lua method '{}' was registered twice", name));
  return {};
}

template <typename T>
template <typename Return, typename... Args, typename Function>
[[nodiscard]] std::expected<void, StarException> LuaMethods<T>::registerMethodWithSignature(String name, Function&& func) {
  if (!m_methods.insert(name, LuaDetail::wrapMethodWithSignature<Return, Args...>(std::forward<Function>(func))).second)
    return std::unexpected(LuaException::format("Lua method '{}' was registered twice", name));
  return {};
}

template <typename T>
StringMap<LuaDetail::LuaWrappedFunction> const& LuaMethods<T>::methods() const {
  return m_methods;
}

template <typename T>
[[nodiscard]] std::expected<T, StarException> LuaContext::getPath(String path) const {
  auto value = engine().contextGetPath(handleIndex(), std::move(path));
  if (!value)
    return std::unexpected(std::move(value.error()));
  return engine().luaTo<T>(std::move(*value));
}

template <typename T>
[[nodiscard]] std::expected<void, StarException> LuaContext::setPath(String key, T value) {
  auto converted = engine().luaFrom(std::move(value));
  if (!converted)
    return std::unexpected(std::move(converted.error()));
  return engine().contextSetPath(handleIndex(), std::move(key), *converted);
}

template <typename Ret>
[[nodiscard]] std::expected<Ret, StarException> LuaContext::eval(String const& lua) {
  auto& eng = engine();
  LuaDetail::LuaStackRestore restore(eng.m_state);
  auto result = eng.contextEval(handleIndex(), lua, LuaDetail::LuaReturn<Ret>::Count);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return LuaDetail::LuaReturn<Ret>::read(LuaStack(eng, eng.m_state), restore.top + 1, *result);
}

template <typename Ret, typename... Args>
[[nodiscard]] std::expected<Ret, StarException> LuaContext::invokePath(String const& key, Args const&... args) const {
  auto& eng = engine();
  LuaDetail::LuaStackRestore restore(eng.m_state);
  auto found = eng.pushContextPath(handleIndex(), key.utf8());
  if (!found)
    return std::unexpected(std::move(found.error()));
  if (!*found)
    return std::unexpected(LuaException::format("invokePath called on path '{}' which is not function type", key));
  auto count = eng.pushArguments(eng.m_state, args...);
  if (!count)
    return std::unexpected(std::move(count.error()));
  return eng.template callStack<Ret>(eng.m_state, static_cast<int>(*count));
}

template <typename Ret, typename... Args>
[[nodiscard]] std::expected<std::optional<Ret>, StarException>
LuaContext::invokePathIfPresent(String const& key, Args const&... args) const {
  auto& eng = engine();
  LuaDetail::LuaStackRestore restore(eng.m_state);
  auto found = eng.pushContextPath(handleIndex(), key.utf8());
  if (!found)
    return std::unexpected(std::move(found.error()));
  if (!*found) {
    if (lua_isnil(eng.m_state, -1))
      return std::optional<Ret>{};
    return std::unexpected(LuaConversionException::format("Lua path '{}' is not a function", key));
  }
  auto count = eng.pushArguments(eng.m_state, args...);
  if (!count)
    return std::unexpected(std::move(count.error()));
  auto result = eng.template callStack<Ret>(eng.m_state, static_cast<int>(*count));
  if (!result)
    return std::unexpected(std::move(result.error()));
  return std::optional<Ret>(std::in_place, std::move(*result));
}

template <typename T>
[[nodiscard]] std::expected<LuaValue, StarException> LuaContext::luaFrom(T&& t) {
  return engine().luaFrom(std::forward<T>(t));
}

template <typename T>
[[nodiscard]] std::expected<LuaValue, StarException> LuaContext::luaFrom(T const& t) {
  return engine().luaFrom(t);
}

template <typename T>
[[nodiscard]] std::expected<std::optional<T>, StarException> LuaContext::luaMaybeTo(LuaValue&& v) {
  return engine().luaMaybeTo<T>(std::move(v));
}

template <typename T>
[[nodiscard]] std::expected<std::optional<T>, StarException> LuaContext::luaMaybeTo(LuaValue const& v) {
  return engine().luaMaybeTo<T>(v);
}

template <typename T>
[[nodiscard]] std::expected<T, StarException> LuaContext::luaTo(LuaValue&& v) {
  return engine().luaTo<T>(std::move(v));
}

template <typename T>
[[nodiscard]] std::expected<T, StarException> LuaContext::luaTo(LuaValue const& v) {
  return engine().luaTo<T>(v);
}

template <typename Container>
[[nodiscard]] std::expected<LuaTable, StarException> LuaContext::createTable(Container const& map) {
  return engine().createTable(map);
}

template <typename Container>
[[nodiscard]] std::expected<LuaTable, StarException> LuaContext::createArrayTable(Container const& array) {
  return engine().createArrayTable(array);
}

template <typename Function>
[[nodiscard]] std::expected<LuaFunction, StarException> LuaContext::createFunction(Function&& func) {
  return engine().createFunction(std::forward<Function>(func));
}

template <typename Return, typename... Args, typename Function>
[[nodiscard]] std::expected<LuaFunction, StarException> LuaContext::createFunctionWithSignature(Function&& func) {
  return engine().createFunctionWithSignature<Return, Args...>(std::forward<Function>(func));
}

template <typename T>
[[nodiscard]] std::expected<LuaUserData, StarException> LuaContext::createUserData(T t) {
  return engine().createUserData(std::move(t));
}

template <typename T>
[[nodiscard]] std::expected<LuaMethods<T>, StarException> LuaUserDataMethods<T>::make() {
  return LuaMethods<T>();
}

template <typename T>
[[nodiscard]] std::expected<void, StarException> LuaUserDataConverter<T>::from(LuaStack stack, T t) {
  return stack.pushUserData(std::move(t));
}

template <typename T>
[[nodiscard]] std::expected<std::optional<T>, StarException> LuaUserDataConverter<T>::to(LuaStackValue v) {
  auto matches = v.isUserData<T>();
  if (!matches)
    return std::unexpected(std::move(matches.error()));
  if (!*matches)
    return std::nullopt;
  auto value = v.userData<T>();
  if (!value)
    return std::unexpected(std::move(value.error()));
  return std::optional<T>(std::in_place, value->get());
}

template <typename T>
[[nodiscard]] std::expected<LuaValue, StarException> LuaEngine::luaFrom(T&& t) {
  if constexpr (std::is_same_v<std::remove_cvref_t<T>, LuaValue> || LuaDetail::LuaValueAlternative<T>) {
    return LuaValue(std::forward<T>(t));
  } else {
    LuaDetail::LuaStackRestore restore(m_state);
    auto pushed = LuaStack(*this, m_state).push(std::forward<T>(t));
    if (!pushed)
      return std::unexpected(std::move(pushed.error()));
    return popLuaValue(m_state);
  }
}

template <typename T>
[[nodiscard]] std::expected<LuaValue, StarException> LuaEngine::luaFrom(T const& t) {
  if constexpr (std::is_same_v<T, LuaValue> || LuaDetail::LuaValueAlternative<T>) {
    return LuaValue(t);
  } else {
    LuaDetail::LuaStackRestore restore(m_state);
    auto pushed = LuaStack(*this, m_state).push(t);
    if (!pushed)
      return std::unexpected(std::move(pushed.error()));
    return popLuaValue(m_state);
  }
}

template <typename T>
[[nodiscard]] std::expected<std::optional<T>, StarException> LuaEngine::luaMaybeTo(LuaValue&& v) {
  if constexpr (std::is_same_v<T, LuaValue>) {
    return std::optional<T>(std::in_place, std::move(v));
  } else {
    if constexpr (LuaDetail::LuaValueAlternative<T>) {
      if (auto value = std::get_if<T>(&v))
        return std::optional<T>(std::in_place, std::move(*value));
    }
    return luaMaybeTo<T>(std::as_const(v));
  }
}

template <typename T>
[[nodiscard]] std::expected<std::optional<T>, StarException> LuaEngine::luaMaybeTo(LuaValue const& v) {
  if constexpr (std::is_same_v<T, LuaValue>) {
    return std::optional<T>(std::in_place, v);
  } else {
    if constexpr (LuaDetail::LuaValueAlternative<T>) {
      if (auto value = std::get_if<T>(&v))
        return std::optional<T>(std::in_place, *value);
    }
    LuaDetail::LuaStackRestore restore(m_state);
    if (auto pushed = pushLuaValue(m_state, v); !pushed)
      return std::unexpected(std::move(pushed.error()));
    return LuaStack(*this, m_state).maybeRead<T>(-1);
  }
}

template <typename T>
[[nodiscard]] std::expected<T, StarException> LuaEngine::luaTo(LuaValue&& v) {
  auto result = luaMaybeTo<T>(std::move(v));
  if (!result)
    return std::unexpected(std::move(result.error()));
  if (!*result)
    return std::unexpected(LuaConversionException::format("Error converting LuaValue to type '{}'", typeName<T>()));
  return std::move(**result);
}

template <typename T>
[[nodiscard]] std::expected<T, StarException> LuaEngine::luaTo(LuaValue const& v) {
  auto result = luaMaybeTo<T>(v);
  if (!result)
    return std::unexpected(std::move(result.error()));
  if (!*result)
    return std::unexpected(LuaConversionException::format("Error converting LuaValue to type '{}'", typeName<T>()));
  return std::move(**result);
}

template <typename Container>
[[nodiscard]] std::expected<LuaTable, StarException> LuaEngine::createTable(Container const& map) {
  LuaDetail::LuaStackRestore restore(m_state);
  LuaStack stack(*this, m_state);
  if (auto pushed = stack.pushMap(map); !pushed)
    return std::unexpected(std::move(pushed.error()));
  return stack.read<LuaTable>(-1);
}

template <typename Container>
[[nodiscard]] std::expected<LuaTable, StarException> LuaEngine::createArrayTable(Container const& array) {
  LuaDetail::LuaStackRestore restore(m_state);
  LuaStack stack(*this, m_state);
  if (auto pushed = stack.pushArray(array); !pushed)
    return std::unexpected(std::move(pushed.error()));
  return stack.read<LuaTable>(-1);
}

template <typename Function>
[[nodiscard]] std::expected<LuaFunction, StarException> LuaEngine::createFunction(Function&& func) {
  return createWrappedFunction(LuaDetail::wrapFunction(std::forward<Function>(func)));
}

template <typename Return, typename... Args, typename Function>
[[nodiscard]] std::expected<LuaFunction, StarException> LuaEngine::createFunctionWithSignature(Function&& func) {
  return createWrappedFunction(LuaDetail::wrapFunctionWithSignature<Return, Args...>(std::forward<Function>(func)));
}

template <typename Ret, typename... Args>
[[nodiscard]] std::expected<Ret, StarException> LuaEngine::callFunction(int handleIndex, Args const&... args) {
  LuaDetail::LuaStackRestore restore(m_state);
  if (!lua_checkstack(m_state, 1))
    return std::unexpected(LuaException("Lua stack overflow while calling function"));
  pushHandle(m_state, handleIndex);
  auto count = pushArguments(m_state, args...);
  if (!count)
    return std::unexpected(std::move(count.error()));
  return callStack<Ret>(m_state, static_cast<int>(*count));
}

template <typename Ret>
[[nodiscard]] std::expected<Ret, StarException> LuaEngine::callStack(lua_State* state, int nargs) {
  int first = lua_gettop(state) - nargs;
  constexpr int count = LuaDetail::LuaReturn<Ret>::Count;
  if (!lua_checkstack(state, count == LUA_MULTRET ? 1 : count + 1))
    return std::unexpected(LuaException("Exhausted the Lua stack while reserving return values"));
  if (auto entered = incrementRecursionLevel(); !entered)
    return std::unexpected(std::move(entered.error()));
  int result = pcallWithTraceback(state, nargs, LuaDetail::LuaReturn<Ret>::Count);
  decrementRecursionLevel();
  if (auto called = handleError(state, result); !called)
    return std::unexpected(std::move(called.error()));
  return LuaDetail::LuaReturn<Ret>::read(LuaStack(*this, state), first, lua_gettop(state) - first + 1);
}

template <typename Ret, typename... Args>
[[nodiscard]] std::expected<std::optional<Ret>, StarException> LuaEngine::resumeThread(int handleIndex,
                                                                                       Args const&... args) {
  LuaDetail::LuaStackRestore restore(m_state);
  if (!lua_checkstack(m_state, 4))
    return std::unexpected(LuaException("Lua stack overflow while resuming thread"));
  pushHandle(m_state, handleIndex);
  lua_State* state = lua_tothread(m_state, -1);
  lua_pop(m_state, 1);
  LuaDetail::LuaStackRestore restoreThread(state);
  if (lua_status(state) != LUA_YIELD && lua_gettop(state) == 0)
    return std::unexpected(LuaException("cannot resume a dead or errored thread"));
  auto count = pushArguments(state, args...);
  if (!count)
    return std::unexpected(std::move(count.error()));
  if (auto entered = incrementRecursionLevel(); !entered)
    return std::unexpected(std::move(entered.error()));
  releaseHandles(state);
  beginDiagnosticScope();
  int results = 0;
  int result = lua_resume(state, m_state, static_cast<int>(*count), &results);
  releaseHandles(state);
  decrementRecursionLevel();
  restoreThread.top = 0;
  if (result != LUA_OK && result != LUA_YIELD)
    propagateErrorWithTraceback(state, m_state);
  endDiagnosticScope();
  if (result != LUA_OK && result != LUA_YIELD) {
    if (auto called = handleError(m_state, result); !called)
      return std::unexpected(std::move(called.error()));
  }
  int first = lua_gettop(state) - results + 1;
  restoreThread.top = first - 1;
  auto value = LuaDetail::LuaReturn<Ret>::read(LuaStack(*this, state), first, results);
  if (!value)
    return std::unexpected(std::move(value.error()));
  return std::optional<Ret>(std::in_place, std::move(*value));
}

template <typename T>
[[nodiscard]] std::expected<void, StarException> LuaEngine::registerUserDataType() {
  if (m_registeredUserDataTypes.contains(typeKey<T>()))
    return {};
  auto methods = LuaUserDataMethods<T>::make();
  if (!methods)
    return std::unexpected(std::move(methods.error()));

  auto table = createTable();
  if (!table)
    return std::unexpected(std::move(table.error()));
  auto index = table->rawSet("__index", *table);
  if (!index)
    return std::unexpected(std::move(index.error()));
  auto protection = table->rawSet("__metatable", false);
  if (!protection)
    return std::unexpected(std::move(protection.error()));
  auto gcFunction = [](lua_State* state) {
    T& t = *(T*)(lua_touserdata(state, 1));
    t.~T();
    return 0;
  };
  auto gc = createRawFunction(gcFunction);
  if (!gc)
    return std::unexpected(std::move(gc.error()));
  auto finalizer = table->rawSet("__gc", std::move(*gc));
  if (!finalizer)
    return std::unexpected(std::move(finalizer.error()));

  for (auto const& p : methods->methods()) {
    auto function = createWrappedFunction(p.second);
    if (!function)
      return std::unexpected(std::move(function.error()));
    auto installed = table->rawSet(p.first, std::move(*function));
    if (!installed)
      return std::unexpected(std::move(installed.error()));
  }

  auto reference = copyHandle(table->handleIndex());
  if (!reference)
    return std::unexpected(std::move(reference.error()));
  auto added = m_registeredUserDataTypes.add(typeKey<T>(), *reference);
  if (!added) {
    destroyHandle(*reference);
    return std::unexpected(std::move(added.error()));
  }
  return {};
}

template <typename T>
[[nodiscard]] std::expected<LuaUserData, StarException> LuaEngine::createUserData(T t) {
  LuaDetail::LuaStackRestore restore(m_state);
  auto pushed = LuaStack(*this, m_state).pushUserData(std::move(t));
  if (!pushed)
    return std::unexpected(std::move(pushed.error()));
  auto reference = popHandle(m_state);
  if (!reference)
    return std::unexpected(std::move(reference.error()));
  return LuaUserData(LuaDetail::LuaHandle(RefPtr<LuaEngine>(this), *reference));
}

template <typename T, typename K>
[[nodiscard]] std::expected<T, StarException> LuaEngine::getGlobal(K key) {
  LuaDetail::LuaStackRestore restore(m_state);
  if (!lua_checkstack(m_state, 1))
    return std::unexpected(LuaException("Lua stack overflow while reading global"));
  pushHandle(m_state, m_scriptDefaultEnvRegistryId);
  return LuaStack(*this, m_state).value(-1).template rawGet<T>(key);
}

template <typename T>
[[nodiscard]] std::expected<T, StarException> LuaEngine::getGlobal(std::string_view key) {
  return getGlobal<T, std::string_view>(key);
}

template <typename T, typename K>
[[nodiscard]] std::expected<void, StarException> LuaEngine::setGlobal(K key, T value) {
  LuaDetail::LuaStackRestore restore(m_state);
  if (!lua_checkstack(m_state, 1))
    return std::unexpected(LuaException("Lua stack overflow while writing global"));
  pushHandle(m_state, m_scriptDefaultEnvRegistryId);
  return LuaStack(*this, m_state).value(-1).rawSet(key, std::move(value));
}

template <typename T>
[[nodiscard]] std::expected<void, StarException> LuaEngine::setGlobal(std::string_view key, T value) {
  return setGlobal<T, std::string_view>(key, std::move(value));
}

template <typename T>
[[nodiscard]] std::expected<bool, StarException> LuaEngine::userDataIsType(int handleIndex) {
  int typeRef = m_registeredUserDataTypes.value(typeKey<T>(), LUA_NOREF);
  if (typeRef == LUA_NOREF)
    return false;
  LuaDetail::LuaStackRestore restore(m_state);
  if (!lua_checkstack(m_state, 3))
    return std::unexpected(LuaException("Lua stack overflow while checking userdata"));
  pushHandle(m_state, handleIndex);
  if (lua_getmetatable(m_state, -1) == 0)
    return std::unexpected(LuaException("Userdata missing metatable in userDataIsType"));
  lua_rawgeti(m_state, LUA_REGISTRYINDEX, typeRef);
  return lua_rawequal(m_state, -1, -2) != 0;
}

template <typename T>
[[nodiscard]] std::expected<std::reference_wrapper<T>, StarException> LuaEngine::getUserData(int handleIndex) {
  int typeRef = m_registeredUserDataTypes.value(typeKey<T>(), LUA_NOREF);
  if (typeRef == LUA_NOREF)
    return std::unexpected(LuaException::format("Cannot convert userdata type of {}, not registered", typeName<T>()));
  LuaDetail::LuaStackRestore restore(m_state);
  if (!lua_checkstack(m_state, 3))
    return std::unexpected(LuaException("Lua stack overflow while accessing userdata"));
  pushHandle(m_state, handleIndex);
  void* userdata = lua_touserdata(m_state, -1);
  if (lua_getmetatable(m_state, -1) == 0)
    return std::unexpected(LuaException("Cannot get userdata from lua type, no metatable found"));
  lua_rawgeti(m_state, LUA_REGISTRYINDEX, typeRef);
  if (!lua_rawequal(m_state, -1, -2) || !userdata)
    return std::unexpected(LuaException::format("Improper conversion from userdata to type {}", typeName<T>()));
  return std::reference_wrapper<T>(*static_cast<T*>(userdata));
}

inline void LuaEngine::destroyHandle(int handleIndex) {
  // Registry references are released later by the engine's handle cleanup.
  m_handleFree.append(handleIndex);
}

template <typename T>
[[nodiscard]] std::expected<std::size_t, StarException> LuaEngine::pushArgument(lua_State* state, T const& arg) {
  if (auto pushed = LuaStack(*this, state).push(arg); !pushed)
    return std::unexpected(std::move(pushed.error()));
  return 1;
}

template <typename T>
[[nodiscard]] std::expected<std::size_t, StarException> LuaEngine::pushArgument(lua_State* state, LuaVariadic<T> const& args) {
  if (args.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    return std::unexpected(LuaException("Too many Lua arguments"));
  LuaDetail::LuaStackRestore restore(state);
  LuaStack stack(*this, state);
  for (auto const& arg : args) {
    if (auto pushed = stack.push(arg); !pushed)
      return std::unexpected(std::move(pushed.error()));
  }
  restore.top = lua_gettop(state);
  return args.size();
}

[[nodiscard]] inline std::expected<std::size_t, StarException> LuaEngine::doPushArguments(lua_State*) {
  return 0;
}

template <typename First, typename... Rest>
[[nodiscard]] std::expected<std::size_t, StarException> LuaEngine::doPushArguments(lua_State* state, First const& first, Rest const&... rest) {
  auto firstSize = pushArgument(state, first);
  if (!firstSize)
    return std::unexpected(std::move(firstSize.error()));
  auto restSize = doPushArguments(state, rest...);
  if (!restSize)
    return std::unexpected(std::move(restSize.error()));
  if (*restSize > static_cast<std::size_t>(std::numeric_limits<int>::max()) - *firstSize)
    return std::unexpected(LuaException("Too many Lua arguments"));
  return *firstSize + *restSize;
}

template <typename... Args>
[[nodiscard]] std::expected<std::size_t, StarException> LuaEngine::pushArguments(lua_State* state, Args const&... args) {
  if (sizeof...(args) > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    return std::unexpected(LuaException("Too many Lua arguments"));
  if (!lua_checkstack(state, static_cast<int>(sizeof...(args))))
    return std::unexpected(LuaException("Lua stack overflow while pushing arguments"));
  LuaDetail::LuaStackRestore restore(state);
  auto count = doPushArguments(state, args...);
  if (!count)
    return std::unexpected(std::move(count.error()));
  restore.top = lua_gettop(state);
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
  struct LuaConverter<LuaThread>;

  template <>
  struct LuaConverter<LuaUserData>;

  template <>
  struct LuaConverter<LuaValue>;

  template <typename T>
  struct LuaConverter<std::optional<T>>;

  template <typename T, typename Allocator>
  struct LuaConverter<List<T, Allocator>>;

  template <typename T, std::size_t MaxSize>
  struct LuaConverter<StaticList<T, MaxSize>>;

  template <typename T, std::size_t MaxStackSize>
  struct LuaConverter<SmallList<T, MaxStackSize>>;

  template <>
  struct LuaConverter<StringList>;

  template <SetContainer Container>
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
  using ::Star::LuaIncompleteStatementException;
  using ::Star::LuaIncompleteStatementExceptionTag;
  using ::Star::LuaInstructionLimitReached;
  using ::Star::LuaInstructionLimitReachedTag;
  using ::Star::LuaInt;
  using ::Star::LuaMethods;
  using ::Star::LuaNil;
  using ::Star::LuaNilType;
  using ::Star::LuaRecursionLimitReached;
  using ::Star::LuaRecursionLimitReachedTag;
  using ::Star::LuaReference;
  using ::Star::LuaStack;
  using ::Star::LuaStackValue;
  using ::Star::LuaString;
  using ::Star::LuaTable;
  using ::Star::LuaThread;
  using ::Star::luaTie;
  using ::Star::LuaTupleReturn;
  using ::Star::luaTupleReturn;
  using ::Star::luaUnpack;
  using ::Star::LuaUserData;
  using ::Star::LuaValue;
  using ::Star::LuaVariadic;
  using ::Star::printLuaValue;
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
  using ::Star::LuaDetail::asInteger;
  using ::Star::LuaDetail::FunctionWrapper;
  using ::Star::LuaDetail::insertJsonMetatable;
  using ::Star::LuaDetail::jarray;
  using ::Star::LuaDetail::jarrayCreate;
  using ::Star::LuaDetail::jcontRemove;
  using ::Star::LuaDetail::jcontResize;
  using ::Star::LuaDetail::jcontSize;
  using ::Star::LuaDetail::jobject;
  using ::Star::LuaDetail::jobjectCreate;
  using ::Star::LuaDetail::jsonContainerToTable;
  using ::Star::LuaDetail::LuaHandle;
  using ::Star::LuaDetail::LuaReturn;
  using ::Star::LuaDetail::LuaWrappedFunction;
  using ::Star::LuaDetail::MethodWrapper;
  using ::Star::LuaDetail::rawSetField;
  using ::Star::LuaDetail::shallowCopy;
  using ::Star::LuaDetail::TableIteratorWrapper;
  using ::Star::LuaDetail::toWrappedReturn;
  using ::Star::LuaDetail::wrapFunction;
  using ::Star::LuaDetail::wrapFunctionArgs;
  using ::Star::LuaDetail::wrapFunctionWithSignature;
  using ::Star::LuaDetail::wrapMethod;
  using ::Star::LuaDetail::wrapMethodArgs;
  using ::Star::LuaDetail::wrapMethodWithSignature;
  using ::Star::LuaDetail::wrapTableIterator;
  using ::Star::LuaDetail::wrapTableIteratorArgs;
  using ::Star::LuaDetail::wrapTableIteratorWithSignature;
  }
}
