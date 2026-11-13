module;
import std;

namespace Star {

using TypeKey = void const*;

namespace type_identity_detail {
template <typename T> inline constexpr unsigned char token = 0;
}

// Every C++ type is a valid identity/name input; do not add a tautological concept.
// Identity is local to one linked image, never persisted or sent on the wire.
template <typename T> constexpr TypeKey type_key() noexcept {
  return &type_identity_detail::token<std::remove_cvref_t<T>>;
}

template <typename T> consteval std::string_view type_name() noexcept {
  constexpr std::string_view signature = std::source_location::current().function_name();
  constexpr auto begin = signature.find("T = ");
  constexpr auto end = signature.rfind(']');
  static_assert(begin != std::string_view::npos && end != std::string_view::npos && end > begin + 4);
  return signature.substr(begin + 4, end - begin - 4);
}

template <typename T>
concept capability_queryable = requires(T const& value, TypeKey key) {
  { value.query_type(key) } noexcept -> std::same_as<void const*>;
};

template <typename Derived, typename Base>
concept capability_base = !std::same_as<Derived, Base> && std::is_convertible_v<Derived const*, Base const*>
  && requires(Base const* base, TypeKey key) {
       { base->Base::query_type(key) } noexcept -> std::same_as<void const*>;
     };

template <typename Derived, typename... Bases>
concept capability_graph = std::is_class_v<Derived> && (capability_base<Derived, Bases> && ...);

// Explicit capabilities, not arbitrary dynamic_cast source-subobject disambiguation.
// self is a live object; the erased address is a non-owning protocol boundary.
template <typename Derived, typename... Bases>
  requires capability_graph<Derived, Bases...>
void const* query_type_of(Derived const* self, TypeKey key) noexcept {
  if (key == type_key<Derived>())
    return self;
  if constexpr (sizeof...(Bases) == 0) {
    return nullptr;
  } else if constexpr (sizeof...(Bases) == 1) {
    return (static_cast<Bases const*>(self)->Bases::query_type(key), ...);
  } else {
    void const* result = nullptr;
    bool ambiguous = false;
    auto merge = [&](void const* candidate) {
      if (candidate) {
        if (result && result != candidate)
          ambiguous = true;
        else
          result = candidate;
      }
    };
    (merge(static_cast<Bases const*>(self)->Bases::query_type(key)), ...);
    return ambiguous ? nullptr : result;
  }
}
}

export module star.type_identity;

export namespace Star {
using ::Star::capability_base;
using ::Star::capability_graph;
using ::Star::capability_queryable;
using ::Star::query_type_of;
using ::Star::type_key;
using ::Star::type_name;
using ::Star::TypeKey;
}
