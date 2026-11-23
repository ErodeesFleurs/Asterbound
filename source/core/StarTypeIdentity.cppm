module;
import std;

namespace Star {

class TypeKey {
public:
  constexpr bool operator==(TypeKey const& other) const noexcept {
    return std::addressof(m_token.get()) == std::addressof(other.m_token.get());
  }

  struct Hash {
    std::size_t operator()(TypeKey key) const noexcept {
      return std::hash<unsigned char const*>{}(std::addressof(key.m_token.get()));
    }
  };

private:
  template <typename T> friend constexpr TypeKey type_key() noexcept;
  explicit constexpr TypeKey(unsigned char const& token) noexcept : m_token(token) {}
  std::reference_wrapper<unsigned char const> m_token;
};

namespace type_identity_detail {
template <typename T> inline constexpr unsigned char token = 0;
}

// Every C++ type is a valid identity/name input; do not add a tautological concept.
// Identity is local to one linked image, never persisted or sent on the wire.
template <typename T> constexpr TypeKey type_key() noexcept {
  return TypeKey(type_identity_detail::token<std::remove_cvref_t<T>>);
}

template <typename T> consteval std::string_view type_name() noexcept {
  constexpr std::string_view signature = std::source_location::current().function_name();
  constexpr auto begin = signature.find("T = ");
  constexpr auto end = signature.rfind(']');
  static_assert(begin != std::string_view::npos && end != std::string_view::npos && end > begin + 4);
  return signature.substr(begin + 4, end - begin - 4);
}

template <typename T> class TypeMatch;

// Only the typed request can store a match. Traversal never erases or rebuilds a
// subobject address. A branch accumulates independently so an ambiguous child
// is a miss in its parent, not an ambiguity across the whole graph.
class TypeQuery {
public:
  template <typename T> bool match(T const& value) noexcept;
  template <typename T> bool match(T const&&) = delete;

  template <typename F> void branch(F const& visit) noexcept {
    class Branch final : public Visitor {
    public:
      explicit Branch(F const& fn) noexcept : m_fn(fn) {}
      void visit(TypeQuery& query) const noexcept override { m_fn(query); }

    private:
      F const& m_fn;
    };
    merge(Branch(visit));
  }

private:
  template <typename> friend class TypeMatch;
  ~TypeQuery() = default;
  class Visitor {
  public:
    virtual void visit(TypeQuery& query) const noexcept = 0;
  };
  explicit TypeQuery(TypeKey key) noexcept : m_key(key) {}
  TypeQuery(TypeQuery const&) = delete;
  virtual void merge(Visitor const& branch) noexcept = 0;
  TypeKey m_key;
};

template <typename T> class TypeMatch final : public TypeQuery {
  static_assert(std::same_as<T, std::remove_cvref_t<T>>);

public:
  TypeMatch() noexcept : TypeQuery(type_key<T>()) {}
  auto result() const noexcept { return m_result; }

private:
  friend class TypeQuery;
  void merge(Visitor const& visit) noexcept override {
    TypeMatch child;
    visit.visit(child);
    if (m_ambiguous || !child.m_result)
      return;
    if (m_result && std::addressof(m_result->get()) != std::addressof(child.m_result->get())) {
      m_result.reset();
      m_ambiguous = true;
    } else {
      m_result = child.m_result;
    }
  }

  std::optional<std::reference_wrapper<T const>> m_result;
  bool m_ambiguous = false;
};

template <typename T> bool TypeQuery::match(T const& value) noexcept {
  if (m_key != type_key<T>())
    return false;
  // Only TypeMatch constructs TypeQuery, with its matching normalized type key.
  static_cast<TypeMatch<T>&>(*this).m_result.emplace(value);
  return true;
}

template <typename T>
concept capability_queryable = requires(T const& value, TypeQuery& query) {
  { value.query_type(query) } noexcept -> std::same_as<void>;
};

template <typename T, typename Source>
  requires std::is_object_v<T> && std::is_lvalue_reference_v<Source&&> && capability_queryable<std::remove_reference_t<Source>>
auto query_type(Source&& value) noexcept {
  TypeMatch<std::remove_cv_t<T>> query;
  std::as_const(value).query_type(query);
  return query.result();
}

template <typename Derived, typename Base>
concept capability_base = !std::same_as<Derived, Base> && std::derived_from<Derived, Base>
  && requires(Base const& base, TypeQuery& query) {
       { base.Base::query_type(query) } noexcept -> std::same_as<void>;
     };

template <typename Derived, typename... Bases>
concept capability_graph = std::is_class_v<Derived> && (capability_base<Derived, Bases> && ...);

// Explicit capabilities, not arbitrary dynamic_cast source-subobject disambiguation.
template <typename Derived, typename... Bases>
  requires capability_graph<Derived, Bases...>
void query_type_of(Derived const& self, TypeQuery& query) noexcept {
  if (query.match(self))
    return;
  if constexpr (sizeof...(Bases) == 1) {
    (static_cast<Bases const&>(self).Bases::query_type(query), ...);
  } else if constexpr (sizeof...(Bases) > 1) {
    (query.branch([&](TypeQuery& child) { static_cast<Bases const&>(self).Bases::query_type(child); }), ...);
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
using ::Star::query_type;
using ::Star::TypeKey;
using ::Star::TypeQuery;
}
