module;
import std;
import star.bytes;
namespace Star {

// To avoid having to specialize std::hash in the std namespace, which is
// slightly annoying, Star type wrappers use Star::hash, which just defaults to
// std::hash.  Star::hash also enables template specialization with a dummy
// Enable parameter.
template <typename T, typename Enable = void>
struct hash : public std::hash<T> {};

inline constexpr void hash_combine(std::size_t& hash, std::size_t comb) noexcept {
  hash ^= comb * 2654435761 + 0x9e3779b9 + (hash << 6) + (hash >> 2);
}

// Test the actual hasher/value cv-ref expressions and the implicit conversion
// consumed by hash_combine, without requiring copyability or regularity.
template <typename Hasher, typename Value>
concept hash_invocable =
  requires(std::size_t& seed) { hash_combine(seed, std::declval<Hasher>()(std::declval<Value>())); };

template <typename T>
concept hashable = requires(T const& value, std::size_t& seed) { hash_combine(seed, Star::hash<T>()(value)); };

// PL hashes numeric bytes/codepoints, preserving the input's signedness. The
// cast is intentionally also usable with enums and explicit conversion types.
template <typename T>
concept pl_hash_byte = requires(T& byte) { static_cast<std::size_t>(byte); };

// Paul Larson hashing algorithm, very very *cheap* hashing function.
class PLHasher {
public:
  constexpr PLHasher(std::size_t initial = 0) noexcept : m_hash(initial) {}

  template <pl_hash_byte T> constexpr void put(T b) noexcept(noexcept(static_cast<std::size_t>(b))) {
    m_hash = m_hash * 101 + static_cast<std::size_t>(b);
  }

  constexpr std::size_t hash() const noexcept { return m_hash; }

private:
  std::size_t m_hash;
};

// Character bytes retain the platform's char signedness, as in PLHasher::put.
inline constexpr std::size_t pl_hash_bytes(std::span<char const> bytes) noexcept {
  PLHasher hasher;
  for (char byte : bytes)
    hasher.put(byte);
  return hasher.hash();
}

// Match the project's optional identity: empty hashes to zero, otherwise the
// payload uses Star::hash (which may differ from std::hash).
template <typename T>
struct hash<std::optional<T>> {
  constexpr std::size_t operator()(std::optional<T> const& value) const
    noexcept(noexcept(hash_combine(std::declval<std::size_t&>(), value ? hasher(*value) : 0)))
    requires requires(std::size_t& seed, Star::hash<T> const& payload_hasher, std::optional<T> const& optional) {
      hash_combine(seed, optional ? payload_hasher(*optional) : 0);
    }
  {
    return value ? hasher(*value) : 0;
  }
  hash<T> hasher;
};

template <typename first_t, typename second_t>
class hash<std::pair<first_t, second_t>> {
private:
  Star::hash<first_t> firstHasher;
  Star::hash<second_t> secondHasher;
  using Pair = std::pair<first_t, second_t>;

public:
  constexpr std::size_t operator()(Pair const& a) const
    noexcept(noexcept(hash_combine(std::declval<std::size_t&>(), firstHasher(a.first)))
             && noexcept(hash_combine(std::declval<std::size_t&>(), secondHasher(a.second))))
    requires requires(std::size_t& seed) {
      hash_combine(seed, firstHasher(a.first));
      hash_combine(seed, secondHasher(a.second));
    }
  {
    std::size_t hashval = firstHasher(a.first);
    hash_combine(hashval, secondHasher(a.second));
    return hashval;
  }
};

template <typename... TTypes>
class hash<std::tuple<TTypes...>> {
private:
  using Tuple = std::tuple<TTypes...>;

  static constexpr bool tuple_hashable = []<std::size_t... I>(std::index_sequence<I...>) {
    return (requires(std::size_t& seed) {
      hash_combine(seed, Star::hash<TTypes>()(std::get<I>(std::declval<Tuple const&>())));
    } && ...);
  }(std::index_sequence_for<TTypes...>{});

  static constexpr bool tuple_nothrow_hashable = []<std::size_t... I>(std::index_sequence<I...>) {
    if constexpr (tuple_hashable)
      return (noexcept(hash_combine(std::declval<std::size_t&>(),
                                    Star::hash<TTypes>()(std::get<I>(std::declval<Tuple const&>()))))
              && ...);
    else
      return false;
  }(std::index_sequence_for<TTypes...>{});

  template <std::size_t N> constexpr std::size_t operator()(Tuple const&) const noexcept { return 0; }

  template <std::size_t N, typename THead, typename... TTail>
  constexpr std::size_t operator()(Tuple const& value) const noexcept(tuple_nothrow_hashable) {
    // Keep each temporary hasher's lifetime within this full expression, before
    // evaluating the tail; the tuple includes a terminal zero unlike hash_of.
    std::size_t hash = Star::hash<THead>()(std::get<N - sizeof...(TTail) - 1>(value));
    hash_combine(hash, operator()<N, TTail...>(value));
    return hash;
  }

public:
  constexpr std::size_t operator()(Tuple const& value) const noexcept(tuple_nothrow_hashable)
    requires tuple_hashable
  {
    return operator()<sizeof...(TTypes), TTypes...>(value);
  }
};

template <typename EnumType>
  requires std::is_enum_v<EnumType>
struct hash<EnumType> : std::hash<std::remove_cv_t<EnumType>> {};

template <hashable T>
constexpr std::size_t
hash_of(T const& t) noexcept(noexcept(hash_combine(std::declval<std::size_t&>(), Star::hash<T>()(t)))) {
  return Star::hash<T>()(t);
}

template <hashable T1, hashable T2, hashable... TL>
constexpr std::size_t hash_of(T1 const& t1, T2 const& t2,
                              TL const&... rest) noexcept(noexcept(hash_of(t1)) && noexcept(hash_of(t2))
                                                          && (noexcept(hash_of(rest)) && ...)) {
  std::size_t hash = hash_of(t1);
  hash_combine(hash, hash_of(t2, rest...));
  return hash;
}
}

export module star.hash;

export namespace Star {
  template <typename T>
  struct hash<std::optional<T>>;
  template <typename first_t, typename second_t>
  struct hash<std::pair<first_t, second_t>>;
  template <typename... TTypes>
  struct hash<std::tuple<TTypes...>>;
  template <typename EnumType>
    requires std::is_enum_v<EnumType>
  struct hash<EnumType>;
}


export namespace Star {
  using ::Star::hash;
  using ::Star::hash_combine;
  using ::Star::hash_invocable;
  using ::Star::hash_of;
  using ::Star::hashable;
  using ::Star::pl_hash_byte;
  using ::Star::pl_hash_bytes;
  using ::Star::PLHasher;
}
