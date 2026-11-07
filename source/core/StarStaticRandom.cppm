module;
import std;
import star.xx_hash;
import star.string;
import star.format;

namespace Star {

// Cross-platform, predictable random number generators based on XXHash.
// Each lambda call ends the lifetime of its ADL append return temporary before
// the next value is appended. A single comma fold of hash_append calls would
// instead retain all such temporaries until the end of the fold expression.

template <typename Hasher, typename... Values>
concept static_random_inputs = (hash_appendable<Hasher, Values> && ...);

// Upper bounds include every array byte, since a raw array need not end in NUL.
// Saturating at 65 avoids overflow and never creates an oversized stack buffer.
template <typename Value> consteval std::size_t static_random_pack_size() {
  if constexpr (hash_append_primitive_value<Value>)
    return sizeof(decltype(hash_encode_primitive(std::declval<Value const&>())));
  else if constexpr (std::is_array_v<Value> && std::same_as<std::remove_extent_t<Value>, char>)
    return std::min(std::extent_v<Value>, std::size_t{65});
  else
    return 65;
}

template <typename... Values> consteval std::size_t static_random_pack_capacity() {
  std::size_t size = 0;
  ((size = size > 64 || static_random_pack_size<Values>() > 64 - size ? 65 : size + static_random_pack_size<Values>()),
   ...);
  return size;
}

template <std::size_t Capacity>
  requires(Capacity <= 64)
struct StaticRandomPack {
  std::array<char, Capacity> bytes;
  std::size_t size = 0;

  template <hash_append_primitive_value Value> void append(Value const& value) {
    auto representation = hash_encode_primitive(value);
    std::memcpy(bytes.data() + size, &representation, sizeof(representation));
    size += sizeof(representation);
  }

  template <std::size_t N> void append(char const (&text)[N]) {
    std::string_view view(text, N);
    auto prefix = view.substr(0, view.find('\0'));
    std::memcpy(bytes.data() + size, prefix.data(), prefix.size());
    size += prefix.size();
  }
};

template <typename T, typename... TL>
  requires static_random_inputs<XXHash32, T, TL...>
std::uint32_t static_random_hash32(T const& value, TL const&... rest) {
  constexpr auto capacity = static_random_pack_capacity<T, TL...>();
  if constexpr (capacity <= 64) {
    StaticRandomPack<capacity> pack;
    pack.append(value);
    (pack.append(rest), ...);
    return xx_hash32(std::span<char const>(pack.bytes.data(), pack.size), 2938728349u);
  } else {
    XXHash32 hash(2938728349u);
    hash_append(hash, value);
    ([&] { hash_append(hash, rest); }(), ...);
    return hash.digest();
  }
}

template <typename T, typename... TL>
  requires static_random_inputs<XXHash64, T, TL...>
std::uint64_t static_random_hash64(T const& value, TL const&... rest) {
  constexpr auto capacity = static_random_pack_capacity<T, TL...>();
  if constexpr (capacity <= 64) {
    StaticRandomPack<capacity> pack;
    pack.append(value);
    (pack.append(rest), ...);
    return xx_hash64(std::span<char const>(pack.bytes.data(), pack.size), 1997293021376312589);
  } else {
    XXHash64 hash(1997293021376312589);
    hash_append(hash, value);
    ([&] { hash_append(hash, rest); }(), ...);
    return hash.digest();
  }
}

template <typename T, typename... TL>
  requires static_random_inputs<XXHash32, T, TL...>
std::uint32_t static_random_u32(T const& data, TL const&... rest) {
  return static_random_hash32(data, rest...);
}

template <typename T, typename... TL>
  requires static_random_inputs<XXHash64, T, TL...>
std::uint64_t static_random_u64(T const& data, TL const&... rest) {
  return static_random_hash64(data, rest...);
}

template <typename T, typename... TL>
  requires static_random_inputs<XXHash32, T, TL...>
std::int32_t static_random_i32(T const& data, TL const&... rest) {
  return static_cast<std::int32_t>(static_random_u32(data, rest...));
}

template <typename T, typename... TL>
  requires static_random_inputs<XXHash64, T, TL...>
std::int32_t static_random_i32_range(std::int32_t min, std::int32_t max, T const& data, TL const&... rest) {
  std::uint64_t denom = static_cast<std::uint64_t>((-1)) / (static_cast<std::uint64_t>((max - min)) + 1);
  return static_cast<std::int32_t>((static_random_u64(data, rest...) / denom + min));
}

template <typename T, typename... TL>
  requires static_random_inputs<XXHash64, T, TL...>
std::uint32_t static_random_u32_range(std::uint32_t min, std::uint32_t max, T const& data, TL const&... rest) {
  std::uint64_t denom = static_cast<std::uint64_t>((-1)) / (static_cast<std::uint64_t>((max - min)) + 1);
  return static_random_u64(data, rest...) / denom + min;
}

template <typename T, typename... TL>
  requires static_random_inputs<XXHash64, T, TL...>
std::int64_t static_random_i64(T const& data, TL const&... rest) {
  return static_cast<std::int64_t>(static_random_u64(data, rest...));
}

// Generates values in the range [0.0, 1.0]
template <typename T, typename... TL>
  requires static_random_inputs<XXHash32, T, TL...>
float static_random_float(T const& data, TL const&... rest) {
  return (static_random_u32(data, rest...) & 0x7fffffff) / 2147483648.0;
}

template <typename T, typename... TL>
  requires static_random_inputs<XXHash32, T, TL...>
float static_random_float_range(float min, float max, T const& data, TL const&... rest) {
  return static_random_float(data, rest...) * (max - min) + min;
}

// Generates values in the range [0.0, 1.0]
template <typename T, typename... TL>
  requires static_random_inputs<XXHash64, T, TL...>
double static_random_double(T const& data, TL const&... rest) {
  return (static_random_u64(data, rest...) & 0x7fffffffffffffff) / 9223372036854775808.0;
}

template <typename T, typename... TL>
  requires static_random_inputs<XXHash64, T, TL...>
double static_random_double_range(double min, double max, T const& data, TL const&... rest) {
  return static_random_double(data, rest...) * (max - min) + min;
}

// std::advance dispatches by iterator_category. Check the operations used by
// that dispatch rather than imposing unrelated standard iterator regularity.
template <typename Iterator>
concept static_random_advance_iterator =
  requires(Iterator& iterator) {
    typename std::iterator_traits<Iterator>::iterator_category;
    typename std::iterator_traits<Iterator>::difference_type;
    ++iterator;
  } && std::is_base_of_v<std::input_iterator_tag, typename std::iterator_traits<Iterator>::iterator_category>
  && (!std::is_base_of_v<std::bidirectional_iterator_tag, typename std::iterator_traits<Iterator>::iterator_category>
      || requires(Iterator& iterator) { --iterator; })
  && (!std::is_base_of_v<std::random_access_iterator_tag, typename std::iterator_traits<Iterator>::iterator_category>
      || requires(Iterator& iterator, typename std::iterator_traits<Iterator>::difference_type offset) {
           iterator += offset;
         });

template <typename Container>
using static_random_container_iterator = std::remove_cvref_t<decltype(std::declval<Container&>().begin())>;

template <typename Container>
concept static_random_container = requires(Container& container) {
  typename Container::value_type;
  container.empty() ? true : false;
  static_cast<std::int32_t>(container.size() - 1);
  container.begin();
} && static_random_advance_iterator<static_random_container_iterator<Container>>;

template <typename Container>
using static_random_container_value =
  std::conditional_t<std::is_const_v<Container>, typename Container::value_type const, typename Container::value_type>;

template <typename Container>
concept static_random_reference_container =
  static_random_container<Container> && requires(static_random_container_iterator<Container>& iterator) {
    std::reference_wrapper<static_random_container_value<Container>>(*iterator);
  };

template <typename Container, typename T, typename... TL>
  requires(!std::is_const_v<Container>)
  && static_random_reference_container<Container> && static_random_inputs<XXHash64, T, TL...>
[[nodiscard]] std::expected<std::reference_wrapper<typename Container::value_type>, StarException>
static_random_from(Container& container, T const& data, TL const&... rest) {
  if (container.empty())
    return std::unexpected(StarException::format("static_random_from called on an empty container"));
  auto iterator = container.begin();
  std::advance(iterator, static_random_i32_range(0, static_cast<std::int32_t>(container.size() - 1), data, rest...));
  return std::reference_wrapper<typename Container::value_type>(*iterator);
}

template <typename Container, typename T, typename... TL>
  requires static_random_reference_container<Container const> && static_random_inputs<XXHash64, T, TL...>
[[nodiscard]] std::expected<std::reference_wrapper<typename Container::value_type const>, StarException>
static_random_from(Container const& container, T const& data, TL const&... rest) {
  if (container.empty())
    return std::unexpected(StarException::format("static_random_from called on an empty container"));
  auto iterator = container.begin();
  std::advance(iterator, static_random_i32_range(0, static_cast<std::int32_t>(container.size() - 1), data, rest...));
  return std::reference_wrapper<typename Container::value_type const>(*iterator);
}

template <typename Container>
concept static_random_value_container = static_random_container<Container const> && requires(Container const& container) {
  std::type_identity_t<typename Container::value_type>{};
  static_random_i32_range(0, container.size() - 1, 0);
} && std::is_convertible_v<decltype(*std::declval<static_random_container_iterator<Container const>&>()), typename Container::value_type>;

template <typename Container, typename T, typename... TL>
  requires static_random_value_container<Container> && static_random_inputs<XXHash64, T, TL...>
typename Container::value_type static_random_value_from(Container const& container, T const& data, TL const&... rest) {
  if (container.empty()) {
    return {};
  } else {
    auto iterator = container.begin();
    std::advance(iterator, static_random_i32_range(0, container.size() - 1, data, rest...));
    return *iterator;
  }
}

template <typename T>
class URBG {
public:
  using Function = std::function<T()>;

  URBG(Function func) : m_func(func) {};

  using result_type = T;
  static constexpr T min() { return std::numeric_limits<T>::min(); };
  static constexpr T max() { return std::numeric_limits<T>::max(); };
  T operator()() { return m_func(); };
private:
  Function m_func;
};

template <typename Container>
concept static_random_shuffle_container =
  requires(Container& container, static_random_container_iterator<Container>& iterator,
           std::remove_cvref_t<decltype(std::declval<Container&>().end())>& end) {
    ++iterator != end ? true : false;
    std::swap(*iterator, *(iterator + 0));
  }
  && std::is_constructible_v<static_random_container_iterator<Container>, static_random_container_iterator<Container>&>;

template <typename Container, typename T, typename... TL>
  requires static_random_shuffle_container<Container> && static_random_inputs<XXHash64, T, TL...>
void static_random_shuffle(Container& container, T const& data, TL const&... rest) {
  auto begin = container.begin();
  auto end = container.end();
  auto iterator = begin;
  for (int i = 1, mix = 0; ++iterator != end; ++i) {
    int offset = static_random_u32_range(0, i, ++mix, data, rest...);
    if (offset != i)
      std::swap(*iterator, *(begin + offset));
  }
}
}

export module star.static_random;

export namespace Star {
using ::Star::static_random_double;
using ::Star::static_random_double_range;
using ::Star::static_random_float;
using ::Star::static_random_float_range;
using ::Star::static_random_from;
using ::Star::static_random_hash32;
using ::Star::static_random_hash64;
using ::Star::static_random_i32;
using ::Star::static_random_i32_range;
using ::Star::static_random_i64;
using ::Star::static_random_shuffle;
using ::Star::static_random_u32;
using ::Star::static_random_u32_range;
using ::Star::static_random_u64;
using ::Star::static_random_value_from;
using ::Star::URBG;
}
