module;
import std;
import star.format;
import star.static_random;
import star.byte_array;
import star.math_common;

namespace Star {

struct RandomExceptionTag {
  static constexpr std::string_view name() { return "RandomException"; }
};
using RandomException = StarError<RandomExceptionTag, StarException>;

[[gnu::cold, gnu::noinline]] inline std::unexpected<StarException> random_parameter_error(std::string_view message) {
  return std::unexpected<StarException>(RandomException(std::string(message)));
}

template <typename Range>
concept random_borrowable_range = static_random_reference_container<Range>;

template <random_borrowable_range Range>
using random_reference_t = static_random_reference<Range>;

template <typename Container>
concept random_value_range = static_random_value_container<Container>;

template <typename Container>
concept random_shuffle_range = std::ranges::random_access_range<Container>
  && std::ranges::common_range<Container> && std::permutable<std::ranges::iterator_t<Container>>;

// Deterministic multiply-with-carry stream. Concurrent access requires external
// synchronization: unsynchronized mutation is a C++ data race.
class RandomSource {
public:
  explicit RandomSource(std::uint64_t seed);
  void seed(RandomSource& source);
  void seed(std::uint64_t seed);
  void mix_seed(RandomSource& source);
  void mix_seed(std::uint64_t seed);

  std::uint32_t next_u32() { return generate_u32(); }
  std::uint64_t next_u64() {
    std::uint64_t value = next_u32();
    value = value << 32;
    return value | next_u32();
  }
  std::int32_t next_i32();
  std::int64_t next_i64();

  using result_type = std::uint64_t;
  static constexpr result_type min() { return 0; }
  static constexpr result_type max() { return std::numeric_limits<result_type>::max(); }
  result_type operator()() { return next_u64(); }

  // Closed unit intervals, including rounded endpoints.
  float uniform_float() {
    return (next_u32() & 0x7fffffff) / 2147483648.0f;
  }
  double uniform_double() {
    return (next_u64() & 0x7fffffffffffffff) / 9223372036854775808.0;
  }
  [[nodiscard]] std::expected<std::int64_t, StarException> uniform_int(std::int64_t max) {
    if (max < 0)
      return random_parameter_error("Maximum uniform_int bound must be nonnegative");
    return static_cast<std::int64_t>(uniform_uint(static_cast<std::uint64_t>(max)));
  }
  std::uint64_t uniform_uint(std::uint64_t max) {
    if (max == std::numeric_limits<std::uint64_t>::max())
      return next_u64();
    auto denominator = std::numeric_limits<std::uint64_t>::max() / (max + 1);
    for (;;) {
      auto value = next_u64() / denominator;
      if (value <= max)
        return value;
    }
  }
  [[nodiscard]] std::expected<std::int64_t, StarException> uniform_int(std::int64_t min, std::int64_t max) {
    if (max < min)
      return random_parameter_error("Maximum uniform_int bound must be >= minimum bound");
    auto width = static_cast<std::uint64_t>(max) - static_cast<std::uint64_t>(min);
    return std::bit_cast<std::int64_t>(static_cast<std::uint64_t>(min) + uniform_uint(width));
  }
  [[nodiscard]] std::expected<std::uint64_t, StarException> uniform_uint(std::uint64_t min, std::uint64_t max) {
    if (max < min)
      return random_parameter_error("Maximum uniform_uint bound must be >= minimum bound");
    return uniform_uint(max - min) + min;
  }
  // Keep literal-bound arithmetic and checked-result elimination in the caller.
  // Classification and overflow interpolation retain their protected boundaries.
  [[nodiscard, gnu::always_inline]] std::expected<float, StarException> uniform_float(float min, float max) {
    auto width = range_width(min, max);
    auto finite = classify_finite(min, max, width);
    if (!finite[0] || !finite[1] || max < min)
      return random_parameter_error("Uniform real bounds must be finite and ordered");
    auto unit = uniform_float();
    return finite[2] ? unit * width + min : interpolate_overflow(min, max, unit);
  }
  [[nodiscard, gnu::always_inline]] std::expected<double, StarException> uniform_double(double min, double max) {
    auto width = range_width(min, max);
    auto finite = classify_finite(min, max, width);
    if (!finite[0] || !finite[1] || max < min)
      return random_parameter_error("Uniform real bounds must be finite and ordered");
    auto unit = uniform_double();
    return finite[2] ? unit * width + min : interpolate_overflow(min, max, unit);
  }
  bool next_bool();

  // Polar Box-Muller, without caching a spare draw. Zero deviation still draws.
  [[nodiscard]] std::expected<float, StarException> normal_float(float stddev = 1.0f, float mean = 0.0f) {
    auto finite = classify_finite(stddev, mean);
    if (!finite[0] || !finite[1] || stddev < 0)
      return random_parameter_error("Normal parameters must be finite with nonnegative deviation");
    float first, second, radius_squared;
    do {
      first = 2 * uniform_float() - 1;
      second = 2 * uniform_float() - 1;
      radius_squared = first * first + second * second;
    } while (radius_squared >= 1 || radius_squared == 0);
    if (stddev == 0)
      return mean;
    float mapping = std::sqrt(-2 * std::log(radius_squared) / radius_squared);
    auto result = first * mapping * stddev + mean;
    if (!is_finite(result))
      return random_parameter_error("Normal result is not finite");
    return result;
  }
  [[nodiscard]] std::expected<double, StarException> normal_double(double stddev = 1.0, double mean = 0.0) {
    auto finite = classify_finite(stddev, mean);
    if (!finite[0] || !finite[1] || stddev < 0)
      return random_parameter_error("Normal parameters must be finite with nonnegative deviation");
    double first, second, radius_squared;
    do {
      first = 2 * uniform_double() - 1;
      second = 2 * uniform_double() - 1;
      radius_squared = first * first + second * second;
    } while (radius_squared >= 1 || radius_squared == 0);
    if (stddev == 0)
      return mean;
    double mapping = std::sqrt(-2 * std::log(radius_squared) / radius_squared);
    auto result = first * mapping * stddev + mean;
    if (!is_finite(result))
      return random_parameter_error("Normal result is not finite");
    return result;
  }
  [[nodiscard]] std::expected<std::int64_t, StarException> stochastic_round(double value);
  void fill_bytes(std::span<char> bytes);
  ByteArray random_bytes(std::size_t size);
  void fill_uniform(std::span<float> values);

  template <random_borrowable_range Container>
  [[nodiscard]] std::expected<random_reference_t<Container>, StarException> sample_ref(Container&& container);
  template <random_value_range Container>
    requires std::is_default_constructible_v<static_random_container_value<Container>>
  [[nodiscard]] std::expected<static_random_container_value<Container>, StarException> sample_value(Container const& container);
  template <random_value_range Container>
    requires std::is_constructible_v<static_random_container_value<Container>, static_random_container_value<Container> const&>
  [[nodiscard]] std::expected<static_random_container_value<Container>, StarException> sample_value(Container const& container, static_random_container_value<Container> const& default_value);

  // Correct full-width URBG input; permutations intentionally differ from the
  // former bounded-output generator, which did not satisfy std::shuffle.
  template <random_shuffle_range Container>
  void shuffle(Container& container);

private:
  template <finite_classifiable Real>
  static Real range_width(Real min, Real max);
  template <finite_classifiable Real>
  [[gnu::noinline]] static Real interpolate_overflow(Real min, Real max, Real unit);

  std::uint32_t generate_u32() {
    std::uint64_t value = 809430660ULL * m_data[++m_index] + m_carry;
    m_carry = value >> 32;
    m_data[m_index] = value;
    return value;
  }
  std::array<std::uint32_t, 256> m_data;
  std::uint32_t m_carry;
  std::uint8_t m_index;
};

#pragma float_control(precise, on, push)
template <finite_classifiable Real>
inline Real RandomSource::range_width(Real min, Real max) {
  // Classification includes this speculative width; nonfinite inputs must not
  // make its computation poison before their own finite flags are inspected.
  return max - min;
}

template <finite_classifiable Real>
[[gnu::noinline]] Real RandomSource::interpolate_overflow(Real min, Real max, Real unit) {
  // Finite opposite-sign endpoints reach this overflow-only branch.
  return (Real{1} - unit) * min + unit * max;
}
#pragma float_control(pop)

// Independently owned synchronized stream. Locks protect the stream, not sampled
// containers or references returned to their elements.
class RandomService {
public:
  [[nodiscard]] static std::expected<std::unique_ptr<RandomService>, StarException> create();
  explicit RandomService(std::uint64_t seed);
  [[nodiscard]] std::expected<void, StarException> seed();
  void seed(std::uint64_t seed);
  [[nodiscard]] std::expected<void, StarException> mix_seed();
  void mix_seed(std::uint64_t seed);
  std::uint32_t next_u32();
  std::uint64_t next_u64();
  std::int32_t next_i32();
  std::int64_t next_i64();
  float uniform_float();
  double uniform_double();
  [[nodiscard]] std::expected<std::int64_t, StarException> uniform_int(std::int64_t max);
  std::uint64_t uniform_uint(std::uint64_t max);
  [[nodiscard]] std::expected<std::int64_t, StarException> uniform_int(std::int64_t min, std::int64_t max);
  [[nodiscard]] std::expected<std::uint64_t, StarException> uniform_uint(std::uint64_t min, std::uint64_t max);
  [[nodiscard]] std::expected<float, StarException> uniform_float(float min, float max);
  [[nodiscard]] std::expected<double, StarException> uniform_double(double min, double max);
  bool next_bool();
  [[nodiscard]] std::expected<float, StarException> normal_float(float stddev = 1.0f, float mean = 0.0f);
  [[nodiscard]] std::expected<double, StarException> normal_double(double stddev = 1.0, double mean = 0.0);
  [[nodiscard]] std::expected<std::int64_t, StarException> stochastic_round(double value);
  void fill_bytes(std::span<char> bytes);
  ByteArray random_bytes(std::size_t size);
  // One lock, identical ordered draws to repeated uniform_float calls.
  void fill_uniform(std::span<float> values);

  template <random_borrowable_range Container>
  [[nodiscard]] std::expected<random_reference_t<Container>, StarException> sample_ref(Container&& container);
  template <random_value_range Container>
    requires std::is_default_constructible_v<static_random_container_value<Container>>
  [[nodiscard]] std::expected<static_random_container_value<Container>, StarException> sample_value(Container const& container);
  template <random_value_range Container>
    requires std::is_constructible_v<static_random_container_value<Container>, static_random_container_value<Container> const&>
  [[nodiscard]] std::expected<static_random_container_value<Container>, StarException> sample_value(Container const& container, static_random_container_value<Container> const& default_value);
  // Always consumes one service next_u64, including an empty container.
  template <random_shuffle_range Container>
  void shuffle(Container& container);
private:
  std::mutex m_mutex;
  RandomSource m_source;
};

template <typename Result, typename Random, static_random_container Container>
  requires (std::same_as<Random, RandomSource> || std::same_as<Random, RandomService>)
    && std::is_constructible_v<Result, static_random_container_reference<Container>>
std::expected<Result, StarException> sample_nonempty(Random& random, Container& container) {
  using Difference = typename std::iterator_traits<static_random_container_iterator<Container>>::difference_type;
  auto max_index = container.size() - 1;
  if (std::cmp_greater(max_index, +std::numeric_limits<Difference>::max()))
    return random_parameter_error("Sample index exceeds iterator difference range");
  auto iterator(container.begin());
  // Only the draw touches the service lock; begin, traversal and construction do not.
  auto index = random.uniform_uint(max_index);
  std::advance(iterator, static_cast<Difference>(index));
  return std::expected<Result, StarException>(std::in_place, *iterator);
}

template <random_borrowable_range Container>
std::expected<random_reference_t<Container>, StarException> RandomSource::sample_ref(Container&& container) {
  if (container.empty())
    return random_parameter_error("Empty container in sample_ref");
  return sample_nonempty<random_reference_t<Container>>(*this, container);
}

template <random_borrowable_range Container>
std::expected<random_reference_t<Container>, StarException> RandomService::sample_ref(Container&& container) {
  if (container.empty())
    return random_parameter_error("Empty container in sample_ref");
  return sample_nonempty<random_reference_t<Container>>(*this, container);
}

template <random_value_range Container>
  requires std::is_default_constructible_v<static_random_container_value<Container>>
std::expected<static_random_container_value<Container>, StarException> RandomSource::sample_value(Container const& container) {
  using Value = static_random_container_value<Container>;
  if (container.empty())
    return std::expected<Value, StarException>(std::in_place);
  return sample_nonempty<Value>(*this, container);
}

template <random_value_range Container>
  requires std::is_constructible_v<static_random_container_value<Container>, static_random_container_value<Container> const&>
std::expected<static_random_container_value<Container>, StarException> RandomSource::sample_value(Container const& container, static_random_container_value<Container> const& default_value) {
  using Value = static_random_container_value<Container>;
  if (container.empty())
    return std::expected<Value, StarException>(std::in_place, default_value);
  return sample_nonempty<Value>(*this, container);
}

template <random_value_range Container>
  requires std::is_default_constructible_v<static_random_container_value<Container>>
std::expected<static_random_container_value<Container>, StarException> RandomService::sample_value(Container const& container) {
  using Value = static_random_container_value<Container>;
  if (container.empty())
    return std::expected<Value, StarException>(std::in_place);
  return sample_nonempty<Value>(*this, container);
}

template <random_value_range Container>
  requires std::is_constructible_v<static_random_container_value<Container>, static_random_container_value<Container> const&>
std::expected<static_random_container_value<Container>, StarException> RandomService::sample_value(Container const& container, static_random_container_value<Container> const& default_value) {
  using Value = static_random_container_value<Container>;
  if (container.empty())
    return std::expected<Value, StarException>(std::in_place, default_value);
  return sample_nonempty<Value>(*this, container);
}

template <random_shuffle_range Container>
void RandomSource::shuffle(Container& container) {
  std::shuffle(std::ranges::begin(container), std::ranges::end(container), *this);
}

template <random_shuffle_range Container>
void RandomService::shuffle(Container& container) {
  RandomSource random(next_u64());
  random.shuffle(container);
}

}

export module star.random;

export namespace Star {
  using ::Star::RandomExceptionTag;
  using ::Star::RandomException;
  using ::Star::random_borrowable_range;
  using ::Star::random_reference_t;
  using ::Star::random_value_range;
  using ::Star::random_shuffle_range;
  using ::Star::RandomSource;
  using ::Star::RandomService;
}
