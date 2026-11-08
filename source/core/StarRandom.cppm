module;
import std;
import star.format;
import star.static_random;
import star.byte_array;

namespace Star {

struct RandomExceptionTag {
  static constexpr std::string_view name() { return "RandomException"; }
};
using RandomException = StarError<RandomExceptionTag, StarException>;

template <typename Range>
concept random_borrowable_range = std::ranges::input_range<Range>
  && std::is_lvalue_reference_v<std::ranges::range_reference_t<Range>>
  && requires(Range& range) {
    typename std::remove_cvref_t<Range>::value_type;
    range.empty() ? 0 : 0;
    range.size() - 1;
    range.begin();
  };

template <random_borrowable_range Range>
using random_reference_t = std::reference_wrapper<std::remove_reference_t<std::ranges::range_reference_t<Range>>>;

// Deterministic random number source.  Uses multiply-with-carry algorithm.
// Much higher quality than the predictable random number generators.  Not
// thread safe (won't crash or anything, but might return less than optimal
// values).
class RandomSource {
public:
  // Consumes an explicit seed; no implicit shared random stream.
  RandomSource(std::uint64_t seed);

  // Re-initializes the random number generator using the given seed.  It is
  // exactly equivalent to constructing a new RandomSource, just using the same
  // buffer.
  void init(RandomSource& source);
  void init(std::uint64_t seed);

  void addEntropy(RandomSource& source);
  void addEntropy(std::uint64_t seed);

  std::uint32_t randu32();
  std::uint64_t randu64();

  std::int32_t randi32();
  std::int64_t randi64();

  // Generates values in the range [0.0, 1.0]
  float randf();
  // Generates values in the range [0.0, 1.0]
  double randd();

  // Random integer from [0, max], max must be >= 0
  std::int64_t randInt(std::int64_t max);
  std::uint64_t randUInt(std::uint64_t max);

  // Random integer from [min, max]
  [[nodiscard]] std::expected<std::int64_t, StarException> randInt(std::int64_t min, std::int64_t max);
  [[nodiscard]] std::expected<std::uint64_t, StarException> randUInt(std::uint64_t min, std::uint64_t max);

  [[nodiscard]] std::expected<float, StarException> randf(float min, float max);
  [[nodiscard]] std::expected<double, StarException> randd(double min, double max);

  bool randb();

  // Generates values via normal distribution with box-muller algorithm
  float nrandf(float stddev = 1.0f, float mean = 0.0f);
  double nrandd(double stddev = 1.0, double mean = 0.0);

  // Round a fractional value statistically towards the floor or ceiling.  For
  // example, if a value is 5.2, 80% of the time it will round to 5, but 20% of
  // the time it will round to 6.
  std::int64_t stochasticRound(double val);

  void randBytes(std::span<char> bytes);
  ByteArray randBytes(std::size_t len);

  // Pick a random value out of a container
  template <typename Container>
    requires random_borrowable_range<Container const>
  [[nodiscard]] std::expected<std::reference_wrapper<typename Container::value_type const>, StarException> randFrom(Container const& container);
  template <random_borrowable_range Container>
  [[nodiscard]] std::expected<random_reference_t<Container>, StarException> randFrom(Container& container);
  template <typename Container>
  typename Container::value_type randValueFrom(Container const& container);
  template <typename Container>
  typename Container::value_type randValueFrom(Container const& container, typename Container::value_type const& defaultVal);

  template <typename Container>
  void shuffle(Container& container);

private:
  std::uint32_t gen32();

  std::uint32_t m_data[256];
  std::uint32_t m_carry;
  std::uint8_t m_index;
};

// An independently owned, synchronized random stream.
class RandomService {
public:
  [[nodiscard]] static std::expected<std::unique_ptr<RandomService>, StarException> create();
  explicit RandomService(std::uint64_t seed);
  [[nodiscard]] std::expected<void, StarException> init();
  void init(std::uint64_t seed);

  [[nodiscard]] std::expected<void, StarException> addEntropy();
  void addEntropy(std::uint64_t seed);

  std::uint32_t randu32();
  std::uint64_t randu64();
  std::int32_t randi32();
  std::int64_t randi64();
  float randf();
  double randd();
  long long randInt(long long max);
  unsigned long long randUInt(unsigned long long max);
  [[nodiscard]] std::expected<long long, StarException> randInt(long long min, long long max);
  [[nodiscard]] std::expected<unsigned long long, StarException> randUInt(unsigned long long min, unsigned long long max);
  [[nodiscard]] std::expected<float, StarException> randf(float min, float max);
  [[nodiscard]] std::expected<double, StarException> randd(double min, double max);
  bool randb();

  float nrandf(float stddev = 1.0f, float mean = 0.0f);
  double nrandd(double stddev = 1.0, double mean = 0.0);

  std::int64_t stochasticRound(double val);

  void randBytes(std::span<char> bytes);
  ByteArray randBytes(std::size_t len);

  template <typename Container>
    requires random_borrowable_range<Container const>
  [[nodiscard]] std::expected<std::reference_wrapper<typename Container::value_type const>, StarException> randFrom(Container const& container);
  template <random_borrowable_range Container>
  [[nodiscard]] std::expected<random_reference_t<Container>, StarException> randFrom(Container& container);
  template <typename Container>
  typename Container::value_type randValueFrom(Container const& container);
  template <typename Container>
  typename Container::value_type randValueFrom(Container const& container, typename Container::value_type const& defaultVal);

  template <typename Container>
  void shuffle(Container& container);
private:
  std::mutex m_mutex;
  RandomSource m_source;
};

template <typename Container>
  requires random_borrowable_range<Container const>
std::expected<std::reference_wrapper<typename Container::value_type const>, StarException> RandomSource::randFrom(Container const& container) {
  if (container.empty())
    return std::unexpected(RandomException("Empty container in randFrom"));

  auto i = container.begin();
  std::advance(i, randUInt(container.size() - 1));
  return std::reference_wrapper<typename Container::value_type const>(*i);
}

template <random_borrowable_range Container>
std::expected<random_reference_t<Container>, StarException> RandomSource::randFrom(Container& container) {
  if (container.empty())
    return std::unexpected(RandomException("Empty container in randFrom"));

  auto i = container.begin();
  std::advance(i, randUInt(container.size() - 1));
  return random_reference_t<Container>(*i);
}

template <typename Container>
  requires random_borrowable_range<Container const>
std::expected<std::reference_wrapper<typename Container::value_type const>, StarException> RandomService::randFrom(Container const& container) {
  if (container.empty())
    return std::unexpected(RandomException("Empty container in randFrom"));

  auto i = container.begin();
  std::advance(i, randUInt(container.size() - 1));
  return std::reference_wrapper<typename Container::value_type const>(*i);
}

template <random_borrowable_range Container>
std::expected<random_reference_t<Container>, StarException> RandomService::randFrom(Container& container) {
  if (container.empty())
    return std::unexpected(RandomException("Empty container in randFrom"));

  auto i = container.begin();
  std::advance(i, randUInt(container.size() - 1));
  return random_reference_t<Container>(*i);
}

template <typename Container>
typename Container::value_type RandomSource::randValueFrom(Container const& container) {
  return randValueFrom(container, typename Container::value_type());
}

template <typename Container>
typename Container::value_type RandomSource::randValueFrom(
    Container const& container, typename Container::value_type const& defaultVal) {
  if (container.empty())
    return defaultVal;

  auto i = container.begin();
  std::advance(i, randInt(container.size() - 1));
  return *i;
}

template <typename Container>
void RandomSource::shuffle(Container& container) {
  std::size_t max = container.size();
  std::shuffle(container.begin(), container.end(), URBG<std::size_t>([this, max]() { return randUInt(max - 1); }));
}

template <typename Container>
typename Container::value_type RandomService::randValueFrom(Container const& container) {
  return randValueFrom(container, typename Container::value_type());
}

template <typename Container>
typename Container::value_type RandomService::randValueFrom(
    Container const& container, typename Container::value_type const& defaultVal) {
  if (container.empty())
    return defaultVal;

  auto i = container.begin();
  std::advance(i, randInt(container.size() - 1));
  return *i;
}

template <typename Container>
void RandomService::shuffle(Container& container) {
  RandomSource random(randu64());
  std::shuffle(container.begin(), container.end(), URBG<std::size_t>([&]() { return static_cast<std::size_t>(random.randu64()); }));
}

}

export module star.random;

export namespace Star {
  using ::Star::RandomExceptionTag;
  using ::Star::RandomException;
  using ::Star::random_borrowable_range;
  using ::Star::random_reference_t;
  using ::Star::RandomSource;
  using ::Star::RandomService;
}

