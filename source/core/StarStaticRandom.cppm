module;
import std;
import star.xx_hash;
import star.string;
import star.format;

namespace Star {

// Cross-platform, predictable random number generators based on XXHash.
// Supports primitive types as well as strings for input data.
// Each folded call also ends the lifetime of any ADL push return temporaries.

template <typename T, typename... TL>
std::uint32_t staticRandomHash32(T const& v, TL const&... rest) {
  XXHash32 hash(2938728349u);
  xxHash32Push(hash, v);
  ([&] { xxHash32Push(hash, rest); }(), ...);
  return hash.digest();
}

template <typename T, typename... TL>
std::uint64_t staticRandomHash64(T const& v, TL const&... rest) {
  XXHash64 hash(1997293021376312589);
  xxHash64Push(hash, v);
  ([&] { xxHash64Push(hash, rest); }(), ...);
  return hash.digest();
}

template <typename T, typename... TL>
std::uint32_t staticRandomU32(T const& d, TL const&... rest) {
  return staticRandomHash32(d, rest...);
}

template <typename T, typename... TL>
std::uint64_t staticRandomU64(T const& d, TL const&... rest) {
  return staticRandomHash64(d, rest...);
}

template <typename T, typename... TL>
std::int32_t staticRandomI32(T const& d, TL const&... rest) {
  return static_cast<std::int32_t>(staticRandomU32(d, rest...));
}

template <typename T, typename... TL>
std::int32_t staticRandomI32Range(std::int32_t min, std::int32_t max, T const& d, TL const&... rest) {
  std::uint64_t denom = static_cast<std::uint64_t>((-1)) / (static_cast<std::uint64_t>((max - min)) + 1);
  return static_cast<std::int32_t>((staticRandomU64(d, rest...) / denom + min));
}

template <typename T, typename... TL>
std::uint32_t staticRandomU32Range(std::uint32_t min, std::uint32_t max, T const& d, TL const&... rest) {
  std::uint64_t denom = static_cast<std::uint64_t>((-1)) / (static_cast<std::uint64_t>((max - min)) + 1);
  return staticRandomU64(d, rest...) / denom + min;
}

template <typename T, typename... TL>
std::int64_t staticRandomI64(T const& d, TL const&... rest) {
  return static_cast<std::int64_t>(staticRandomU64(d, rest...));
}

// Generates values in the range [0.0, 1.0]
template <typename T, typename... TL>
float staticRandomFloat(T const& d, TL const&... rest) {
  return (staticRandomU32(d, rest...) & 0x7fffffff) / 2147483648.0;
}

template <typename T, typename... TL>
float staticRandomFloatRange(float min, float max, T const& d, TL const&... rest) {
  return staticRandomFloat(d, rest...) * (max - min) + min;
}

// Generates values in the range [0.0, 1.0]
template <typename T, typename... TL>
double staticRandomDouble(T const& d, TL const&... rest) {
  return (staticRandomU64(d, rest...) & 0x7fffffffffffffff) / 9223372036854775808.0;
}

template <typename T, typename... TL>
double staticRandomDoubleRange(double min, double max, T const& d, TL const&... rest) {
  return staticRandomDouble(d, rest...) * (max - min) + min;
}

template <typename Container, typename T, typename... TL>
[[nodiscard]] std::expected<std::reference_wrapper<typename Container::value_type>, StarException> staticRandomFrom(Container& container, T const& d, TL const&... rest) {
  if (container.empty())
    return std::unexpected(StarException::format("staticRandomFrom called on an empty container"));
  auto i = container.begin();
  std::advance(i, staticRandomI32Range(0, static_cast<std::int32_t>((container.size() - 1)), d, rest...));
  return std::reference_wrapper<typename Container::value_type>(*i);
}

template <typename Container, typename T, typename... TL>
[[nodiscard]] std::expected<std::reference_wrapper<typename Container::value_type const>, StarException> staticRandomFrom(Container const& container, T const& d, TL const&... rest) {
  if (container.empty())
    return std::unexpected(StarException::format("staticRandomFrom called on an empty container"));
  auto i = container.begin();
  std::advance(i, staticRandomI32Range(0, static_cast<std::int32_t>((container.size() - 1)), d, rest...));
  return std::reference_wrapper<typename Container::value_type const>(*i);
}

template <typename Container, typename T, typename... TL>
typename Container::value_type staticRandomValueFrom(Container const& container, T const& d, TL const&... rest) {
  if (container.empty()) {
    return {};
  } else {
    auto i = container.begin();
    std::advance(i, staticRandomI32Range(0, container.size() - 1, d, rest...));
    return *i;
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

template <typename Container, typename T, typename... TL>
void staticRandomShuffle(Container& container, T const& d, TL const&... rest) {
  auto begin = container.begin();
  auto end = container.end();
  auto it = begin;
  for (int i = 1, mix = 0; ++it != end; ++i) {
    int off = staticRandomU32Range(0, i, ++mix, d, rest...);
    if (off != i)
      std::swap(*it, *(begin + off));    
  }
}

}

export module star.static_random;

export namespace Star {
  using ::Star::staticRandomHash32;
  using ::Star::staticRandomHash64;
  using ::Star::staticRandomU32;
  using ::Star::staticRandomU64;
  using ::Star::staticRandomI32;
  using ::Star::staticRandomI32Range;
  using ::Star::staticRandomU32Range;
  using ::Star::staticRandomI64;
  using ::Star::staticRandomFloat;
  using ::Star::staticRandomFloatRange;
  using ::Star::staticRandomDouble;
  using ::Star::staticRandomDoubleRange;
  using ::Star::staticRandomFrom;
  using ::Star::staticRandomValueFrom;
  using ::Star::URBG;
  using ::Star::staticRandomShuffle;
}
