module;

import std;
import star.format;
import star.rect;
import star.vector;
import star.string;
import star.optional;
import star.lua;
import star.color;
import star.poly;
import star.line;
import star.rpc_promise;

import star.matrix3;
namespace Star {

template <typename T1, typename T2>
struct LuaConverter<std::pair<T1, T2>> {
  template <typename Pair> [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, Pair&& v) {
    auto created = stack.createTable(2, 0);
    if (!created)
      return std::unexpected(std::move(created.error()));
    auto t = std::move(*created);
    auto first = t.set(1, std::forward<Pair>(v).first);
    if (!first)
      return std::unexpected(std::move(first.error()));
    auto second = t.set(2, std::forward<Pair>(v).second);
    if (!second)
      return std::unexpected(std::move(second.error()));
    return {};
  }

  [[nodiscard]] static std::expected<std::optional<std::pair<T1, T2>>, StarException> to(LuaStackValue v) {
    if (!v.is<LuaTable>())
      return std::nullopt;
    auto p1 = v.template maybeGet<T1>(1);
    if (!p1)
      return std::unexpected(std::move(p1.error()));
    if (!*p1)
      return std::nullopt;
    auto p2 = v.template maybeGet<T2>(2);
    if (!p2)
      return std::unexpected(std::move(p2.error()));
    if (!*p2)
      return std::nullopt;
    return std::optional<std::pair<T1, T2>>(std::in_place, std::move(**p1), std::move(**p2));
  }
};

template <typename T, std::size_t N>
struct LuaConverter<Vector<T, N>> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, Vector<T, N> const& v) {
    return stack.pushArray(v);
  }

  [[nodiscard]] static std::expected<std::optional<Vector<T, N>>, StarException> to(LuaStackValue v) {
    if (!v.is<LuaTable>())
      return std::nullopt;
    Vector<T, N> vec;
    for (std::size_t i = 0; i < N; ++i) {
      auto converted = v.template maybeGet<T>(i + 1);
      if (!converted)
        return std::unexpected(std::move(converted.error()));
      if (!*converted)
        return std::nullopt;
      vec[i] = std::move(**converted);
    }
    return vec;
  }
};

template <typename T>
struct LuaConverter<Matrix3<T>> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, Matrix3<T> const& m) {
    auto created = stack.createTable(3, 0);
    if (!created)
      return std::unexpected(std::move(created.error()));
    auto table = std::move(*created);
    for (std::size_t i = 0; i < 3; ++i) {
      auto result = table.set(i + 1, m[i]);
      if (!result)
        return std::unexpected(std::move(result.error()));
    }
    return {};
  }

  [[nodiscard]] static std::expected<std::optional<Matrix3<T>>, StarException> to(LuaStackValue v) {
    auto rows = LuaConverter<Vector<typename Matrix3<T>::Vec3, 3>>::to(v);
    if (!rows)
      return std::unexpected(std::move(rows.error()));
    if (!*rows)
      return std::nullopt;
    return Matrix3<T>((**rows)[0], (**rows)[1], (**rows)[2]);
  }
};

template <typename T>
struct LuaConverter<Rect<T>> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, Rect<T> const& v) {
    if (v.isNull())
      return stack.push(LuaNil);
    return stack.pushArray(Vector<T, 4>(v.xMin(), v.yMin(), v.xMax(), v.yMax()));
  }

  [[nodiscard]] static std::expected<std::optional<Rect<T>>, StarException> to(LuaStackValue v) {
    if (v.is<LuaNilType>())
      return Rect<T>::null();
    auto bounds = LuaConverter<Vector<T, 4>>::to(v);
    if (!bounds)
      return std::unexpected(std::move(bounds.error()));
    if (!*bounds)
      return std::nullopt;
    return Rect<T>((**bounds)[0], (**bounds)[1], (**bounds)[2], (**bounds)[3]);
  }
};

template <typename T>
struct LuaConverter<Polygon<T>> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, Polygon<T> const& poly) {
    return stack.pushArray(poly.vertexes());
  }

  [[nodiscard]] static std::expected<std::optional<Polygon<T>>, StarException> to(LuaStackValue v) {
    auto points = v.template maybeTo<typename Polygon<T>::VertexList>();
    if (!points)
      return std::unexpected(std::move(points.error()));
    if (!*points)
      return std::nullopt;
    return Polygon<T>(std::move(**points));
  }
};

template <typename T, std::size_t N>
struct LuaConverter<Line<T, N>> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, Line<T, N> const& line) {
    return LuaConverter<std::pair<Vector<T, N>, Vector<T, N>>>::from(stack, std::pair(line.min(), line.max()));
  }

  [[nodiscard]] static std::expected<std::optional<Line<T, N>>, StarException> to(LuaStackValue v) {
    auto endpoints = LuaConverter<std::pair<Vector<T, N>, Vector<T, N>>>::to(v);
    if (!endpoints)
      return std::unexpected(std::move(endpoints.error()));
    if (!*endpoints)
      return std::nullopt;
    return Line<T, N>((**endpoints).first, (**endpoints).second);
  }
};

// Probe alternatives in declaration order; failed probes must not consume v.
// A leading monostate represents successful nil, not conversion failure.
template <typename... Types>
struct LuaConverter<std::variant<Types...>> {
  using Value = std::variant<Types...>;

  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, Value const& value) {
    return std::visit(
      [stack](auto const& payload) mutable -> std::expected<void, StarException> {
        if constexpr (std::is_same_v<std::remove_cvref_t<decltype(payload)>, std::monostate>)
          return stack.push(LuaNil);
        else
          return stack.push(payload);
      },
      value);
  }

  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, Value&& value) {
    return std::visit(
      [stack](auto& payload) mutable -> std::expected<void, StarException> {
        if constexpr (std::is_same_v<std::remove_cvref_t<decltype(payload)>, std::monostate>)
          return stack.push(LuaNil);
        else
          return stack.push(std::move(payload));
      },
      value);
  }

  template <std::size_t Index = 0>
  [[nodiscard]] static std::expected<std::optional<Value>, StarException> checkTypeTo(LuaStackValue v) {
    using Type = std::variant_alternative_t<Index, Value>;
    if constexpr (std::is_same_v<Type, std::monostate>) {
      if (v.is<LuaNilType>())
        return std::optional<Value>(std::in_place, std::in_place_index<Index>);
    } else {
      auto converted = v.template maybeTo<Type>();
      if (!converted)
        return std::unexpected(std::move(converted.error()));
      if (*converted)
        return std::optional<Value>(std::in_place, std::in_place_index<Index>, std::move(**converted));
    }
    if constexpr (Index + 1 < sizeof...(Types))
      return checkTypeTo<Index + 1>(v);
    else
      return std::nullopt;
  }

  [[nodiscard]] static std::expected<std::optional<Value>, StarException> to(LuaStackValue v) { return checkTypeTo(v); }
};

template <>
struct LuaConverter<Color> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, Color const& c);
  [[nodiscard]] static std::expected<std::optional<Color>, StarException> to(LuaStackValue v);
};

template <>
struct LuaConverter<LuaCallbacks> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, LuaCallbacks const& c);
  [[nodiscard]] static std::expected<std::optional<LuaCallbacks>, StarException> to(LuaStackValue v);
};

template <typename T> struct LuaConverter<RpcPromise<T>> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, RpcPromise<T> const& promise) {
    auto scoped = promise.withCancellation(stack.engine().promiseLifetime(), "Lua session stopped");
    if (!scoped)
      return std::unexpected(std::move(scoped.error()));
    return LuaUserDataConverter<RpcPromise<T>>::from(stack, std::move(*scoped));
  }
  [[nodiscard]] static std::expected<std::optional<RpcPromise<T>>, StarException> to(LuaStackValue value) {
    auto promise = LuaUserDataConverter<RpcPromise<T>>::to(value);
    if (!promise || !*promise)
      return promise;
    auto scoped = (**promise).withCancellation(value.stack().engine().promiseLifetime(), "Lua session stopped");
    if (!scoped)
      return std::unexpected(std::move(scoped.error()));
    return std::optional<RpcPromise<T>>(std::move(*scoped));
  }
};

template <typename T>
struct LuaUserDataMethods<RpcPromise<T>> {
  [[nodiscard]] static std::expected<LuaMethods<RpcPromise<T>>, StarException> make();
};

template <typename T>
[[nodiscard]] std::expected<LuaMethods<RpcPromise<T>>, StarException> LuaUserDataMethods<RpcPromise<T>>::make() {
  LuaMethods<RpcPromise<T>> methods;
  if (auto registered =
        methods.registerMethod("status",
                               [](RpcPromise<T>& promise) -> std::expected<std::string_view, StarException> {
                                 auto status = promise.status();
                                 if (!status)
                                   return std::unexpected(std::move(status.error()));
                                 switch (*status) {
                                 case RpcPromiseStatus::Pending: return "pending";
                                 case RpcPromiseStatus::Succeeded: return "succeeded";
                                 case RpcPromiseStatus::Failed: return "failed";
                                 case RpcPromiseStatus::Cancelled: return "cancelled";
                                 }
                                 std::unreachable();
                               });
      !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = methods.registerMethod("failed", [](RpcPromise<T>& promise) { return promise.failed(); });
      !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = methods.registerMethod("cancelled", [](RpcPromise<T>& promise) { return promise.cancelled(); });
      !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered =
        methods.registerMethod("cancel", [](RpcPromise<T>& promise) { return promise.cancel("Cancelled"); });
      !registered)
    return std::unexpected(std::move(registered.error()));
  auto finished = methods.template registerMethodWithSignature<bool, RpcPromise<T>&>("finished", std::mem_fn(&RpcPromise<T>::finished));
  if (!finished)
    return std::unexpected(std::move(finished.error()));
  auto succeeded = methods.template registerMethodWithSignature<bool, RpcPromise<T>&>("succeeded", std::mem_fn(&RpcPromise<T>::succeeded));
  if (!succeeded)
    return std::unexpected(std::move(succeeded.error()));
  auto result = methods.template registerMethodWithSignature<std::optional<T>, RpcPromise<T>&>("result", std::mem_fn(&RpcPromise<T>::result));
  if (!result)
    return std::unexpected(std::move(result.error()));
  auto error = methods.template registerMethodWithSignature<std::optional<String>, RpcPromise<T>&>("error", std::mem_fn(&RpcPromise<T>::error));
  if (!error)
    return std::unexpected(std::move(error.error()));
  return methods;
}

// all RpcPromiseKeepers should be wrapped in this before being passed to Lua
template <typename T>
struct LuaConverter<std::shared_ptr<AutoFailRpcPromiseKeeper<T>>> : LuaUserDataConverter<std::shared_ptr<AutoFailRpcPromiseKeeper<T>>> {};

template <typename T>
struct LuaUserDataMethods<std::shared_ptr<AutoFailRpcPromiseKeeper<T>>> {
  [[nodiscard]] static std::expected<LuaMethods<std::shared_ptr<AutoFailRpcPromiseKeeper<T>>>, StarException> make();
};

template <typename T>
[[nodiscard]] std::expected<LuaMethods<std::shared_ptr<AutoFailRpcPromiseKeeper<T>>>, StarException> LuaUserDataMethods<std::shared_ptr<AutoFailRpcPromiseKeeper<T>>>::make() {
  LuaMethods<std::shared_ptr<AutoFailRpcPromiseKeeper<T>>> methods;
  auto chain = methods.template registerMethodWithSignature<void, std::shared_ptr<AutoFailRpcPromiseKeeper<T>>, RpcPromise<T>>("chain", [](std::shared_ptr<AutoFailRpcPromiseKeeper<T>> const& keeper, RpcPromise<T> const& promise) {
    return keeper->chain(promise);
  });
  if (!chain)
    return std::unexpected(std::move(chain.error()));
  auto fulfill = methods.template registerMethodWithSignature<void, std::shared_ptr<AutoFailRpcPromiseKeeper<T>>, T>("fulfill", [](std::shared_ptr<AutoFailRpcPromiseKeeper<T>> const& keeper, T const& result) {
    return keeper->fulfill(result);
  });
  if (!fulfill)
    return std::unexpected(std::move(fulfill.error()));
  auto fail = methods.template registerMethodWithSignature<void, std::shared_ptr<AutoFailRpcPromiseKeeper<T>>, String>("fail", [](std::shared_ptr<AutoFailRpcPromiseKeeper<T>> const& keeper, String const& error) {
    return keeper->fail(error);
  });
  if (!fail)
    return std::unexpected(std::move(fail.error()));
  if (auto registered = methods.registerMethod(
        "cancel",
        [](std::shared_ptr<AutoFailRpcPromiseKeeper<T>> const& keeper) { return keeper->cancel("Cancelled"); });
      !registered)
    return std::unexpected(std::move(registered.error()));
  return methods;
}

}

export module star.lua_converters;

export namespace Star {
  using ::Star::LuaConverter;
  using ::Star::LuaUserDataMethods;

  template <typename T1, typename T2>
  struct LuaConverter<std::pair<T1, T2>>;
  template <typename T, std::size_t N>
  struct LuaConverter<Vector<T, N>>;
  template <typename T>
  struct LuaConverter<Matrix3<T>>;
  template <typename T>
  struct LuaConverter<Rect<T>>;
  template <typename T>
  struct LuaConverter<Polygon<T>>;
  template <typename T, std::size_t N>
  struct LuaConverter<Line<T, N>>;
  template <typename... Types>
  struct LuaConverter<std::variant<Types...>>;
  template <>
  struct LuaConverter<Color>;
  template <>
  struct LuaConverter<LuaCallbacks>;
  template <typename T>
  struct LuaConverter<RpcPromise<T>>;
  template <typename T>
  struct LuaUserDataMethods<RpcPromise<T>>;
  template <typename T>
  struct LuaConverter<std::shared_ptr<AutoFailRpcPromiseKeeper<T>>>;
  template <typename T>
  struct LuaUserDataMethods<std::shared_ptr<AutoFailRpcPromiseKeeper<T>>>;
}
