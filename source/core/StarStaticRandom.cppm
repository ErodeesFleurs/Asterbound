module;
import std;
import star.xx_hash;
import star.string;
import star.format;
import star.math_common;

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

// Keep accepted quotient mappings. Retrying hashes the original digest and a
// canonical counter, without re-evaluating user-defined ADL append operations.
inline std::uint64_t static_random_bounded(std::uint64_t candidate, std::uint64_t max) {
  if (max == std::numeric_limits<std::uint64_t>::max())
    return candidate;
  auto original = candidate;
  auto denominator = std::numeric_limits<std::uint64_t>::max() / (max + 1);
  std::uint64_t retry = 0;
  for (;;) {
    auto value = candidate / denominator;
    if (value <= max)
      return value;
    candidate = static_random_u64(original, ++retry);
  }
}

template <typename T, typename... TL>
  requires static_random_inputs<XXHash64, T, TL...>
[[nodiscard]] std::expected<std::int32_t, StarException>
static_random_i32_range(std::int32_t min, std::int32_t max, T const& data, TL const&... rest) {
  if (max < min)
    return std::unexpected(StarException("Maximum static integer bound must be >= minimum bound"));
  auto width = static_cast<std::uint32_t>(max) - static_cast<std::uint32_t>(min);
  auto offset = static_random_bounded(static_random_u64(data, rest...), width);
  return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(min) + static_cast<std::uint32_t>(offset));
}

template <typename T, typename... TL>
  requires static_random_inputs<XXHash64, T, TL...>
[[nodiscard]] std::expected<std::uint32_t, StarException>
static_random_u32_range(std::uint32_t min, std::uint32_t max, T const& data, TL const&... rest) {
  if (max < min)
    return std::unexpected(StarException("Maximum static integer bound must be >= minimum bound"));
  return static_cast<std::uint32_t>(static_random_bounded(static_random_u64(data, rest...), max - min)) + min;
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

// Only the overflowing-width fallback changes floating-point policy. Ordinary
// arithmetic retains the production flags and historical evaluation.
#pragma float_control(precise, on, push)
template <std::floating_point Real>
[[gnu::noinline]] Real static_random_interpolate_overflow(Real min, Real max, Real unit) {
  // Finite opposite-sign endpoints reach this overflow-only branch.
  return (Real{1} - unit) * min + unit * max;
}
#pragma float_control(pop)

template <typename T, typename... TL>
  requires static_random_inputs<XXHash32, T, TL...>
[[nodiscard]] std::expected<float, StarException>
static_random_float_range(float min, float max, T const& data, TL const&... rest) {
  if (!is_finite(min) || !is_finite(max) || max < min)
    return std::unexpected(StarException("Static real bounds must be finite and ordered"));
  auto unit = static_random_float(data, rest...);
  auto width = max - min;
  return is_finite(width) ? unit * width + min : static_random_interpolate_overflow(min, max, unit);
}

// Generates values in the range [0.0, 1.0]
template <typename T, typename... TL>
  requires static_random_inputs<XXHash64, T, TL...>
double static_random_double(T const& data, TL const&... rest) {
  return (static_random_u64(data, rest...) & 0x7fffffffffffffff) / 9223372036854775808.0;
}

template <typename T, typename... TL>
  requires static_random_inputs<XXHash64, T, TL...>
[[nodiscard]] std::expected<double, StarException>
static_random_double_range(double min, double max, T const& data, TL const&... rest) {
  if (!is_finite(min) || !is_finite(max) || max < min)
    return std::unexpected(StarException("Static real bounds must be finite and ordered"));
  auto unit = static_random_double(data, rest...);
  auto width = max - min;
  return is_finite(width) ? unit * width + min : static_random_interpolate_overflow(min, max, unit);
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
  && (std::is_base_of_v<std::random_access_iterator_tag, typename std::iterator_traits<Iterator>::iterator_category>
      || requires(typename std::iterator_traits<Iterator>::difference_type count) { --count; })
  && (!std::is_base_of_v<std::bidirectional_iterator_tag, typename std::iterator_traits<Iterator>::iterator_category>
      || requires(Iterator& iterator) { --iterator; })
  && (!std::is_base_of_v<std::random_access_iterator_tag, typename std::iterator_traits<Iterator>::iterator_category>
      || requires(Iterator& iterator, typename std::iterator_traits<Iterator>::difference_type offset) {
           iterator += offset;
         });

template <typename Container>
using static_random_container_iterator = std::remove_cvref_t<decltype(std::declval<Container&>().begin())>;

template <typename Container>
using static_random_container_reference = decltype(*std::declval<static_random_container_iterator<Container>&>());

template <typename Container>
using static_random_container_value = std::iter_value_t<static_random_container_iterator<Container>>;

template <typename Container>
concept static_random_sized_container = requires(Container& container) {
  container.empty() ? true : false;
  requires std::integral<std::remove_cvref_t<decltype(container.size())>>;
  requires std::integral<typename std::iterator_traits<static_random_container_iterator<Container>>::difference_type>;
  static_random_container_iterator<Container>(container.begin());
};

template <typename Container>
concept static_random_container = static_random_sized_container<Container>
  && static_random_advance_iterator<static_random_container_iterator<Container>>;

template <typename Container>
concept static_random_reference_container = static_random_container<Container>
  && std::is_lvalue_reference_v<static_random_container_reference<Container>>
  && (std::is_lvalue_reference_v<Container> || std::ranges::borrowed_range<Container>);

template <typename Container>
using static_random_reference = std::reference_wrapper<std::remove_reference_t<static_random_container_reference<Container>>>;

[[gnu::cold, gnu::noinline]] inline std::unexpected<StarException> static_random_selection_error(std::string_view message) {
  return std::unexpected(StarException(std::string(message)));
}

template <typename Result, static_random_container Container, typename T, typename... TL>
  requires std::is_constructible_v<Result, static_random_container_reference<Container>>
    && static_random_inputs<XXHash64, T, TL...>
std::expected<Result, StarException> static_random_select_nonempty(Container& container, T const& data, TL const&... rest) {
  using Difference = typename std::iterator_traits<static_random_container_iterator<Container>>::difference_type;
  auto max_index = container.size() - 1;
  if (std::cmp_greater(max_index, std::numeric_limits<std::int32_t>::max()))
    return static_random_selection_error("Static selection index exceeds int32 range");
  if (std::cmp_greater(max_index, +std::numeric_limits<Difference>::max()))
    return static_random_selection_error("Static selection index exceeds iterator difference range");
  auto index = static_random_i32_range(0, static_cast<std::int32_t>(max_index), data, rest...);
  if (!index)
    return std::unexpected(std::move(index.error()));
  auto iterator(container.begin());
  std::advance(iterator, *index);
  return std::expected<Result, StarException>(std::in_place, *iterator);
}

template <typename Container, typename T, typename... TL>
  requires static_random_reference_container<Container> && static_random_inputs<XXHash64, T, TL...>
[[nodiscard]] std::expected<static_random_reference<Container>, StarException>
static_random_from(Container&& container, T const& data, TL const&... rest) {
  if (container.empty())
    return static_random_selection_error("static_random_from called on an empty container");
  return static_random_select_nonempty<static_random_reference<Container>>(container, data, rest...);
}

template <typename Container>
concept static_random_value_container = static_random_container<Container const>
  && std::is_constructible_v<static_random_container_value<Container>, static_random_container_reference<Container const>>;

template <typename Container, typename T, typename... TL>
  requires static_random_value_container<Container> && static_random_inputs<XXHash64, T, TL...>
[[nodiscard]] std::expected<static_random_container_value<Container>, StarException>
static_random_value_from(Container const& container, T const& data, TL const&... rest) {
  using Value = static_random_container_value<Container>;
  if (container.empty()) {
    if constexpr (std::is_default_constructible_v<Value>)
      return std::expected<Value, StarException>(std::in_place);
    else
      return static_random_selection_error("static_random_value_from called on an empty container");
  }
  return static_random_select_nonempty<Value>(container, data, rest...);
}

template <typename Container>
concept static_random_shuffle_container = static_random_sized_container<Container>
  && requires(Container& container, static_random_container_iterator<Container>& iterator, std::uint32_t& offset) {
    container.end();
    ++iterator != container.end() ? true : false;
    std::swap(*iterator, *(iterator + offset));
  }
  && std::is_constructible_v<static_random_container_iterator<Container>, static_random_container_iterator<Container>&>;

template <typename Container, typename T, typename... TL>
  requires static_random_shuffle_container<Container> && static_random_inputs<XXHash64, int, T, TL...>
[[nodiscard]] std::expected<void, StarException>
static_random_shuffle(Container& container, T const& data, TL const&... rest) {
  if (container.empty())
    return {};
  // The historical mix input is an int, including its hash encoding.
  auto max_index = container.size() - 1;
  if (std::cmp_greater(max_index, std::numeric_limits<int>::max()))
    return static_random_selection_error("Static shuffle index exceeds int range");
  using Difference = typename std::iterator_traits<static_random_container_iterator<Container>>::difference_type;
  if (std::cmp_greater(max_index, +std::numeric_limits<Difference>::max()))
    return static_random_selection_error("Static shuffle index exceeds iterator difference range");
  auto begin(container.begin());
  auto iterator(begin);
  for (int i = 1; ++iterator != container.end(); ++i) {
    auto offset = static_random_u32_range(0, static_cast<std::uint32_t>(i), i, data, rest...);
    if (!offset)
      return std::unexpected(std::move(offset.error()));
    if (*offset != static_cast<std::uint32_t>(i))
      std::swap(*iterator, *(begin + *offset));
    if (i == std::numeric_limits<int>::max())
      break;
  }
  return {};
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
using ::Star::static_random_bounded;
using ::Star::static_random_container;
using ::Star::static_random_container_iterator;
using ::Star::static_random_container_reference;
using ::Star::static_random_container_value;
using ::Star::static_random_reference_container;
using ::Star::static_random_reference;
using ::Star::static_random_value_container;
using ::Star::static_random_shuffle_container;
}
