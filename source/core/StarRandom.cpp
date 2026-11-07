import std;
import star.format;
import star.xx_hash;
import star.math_common;

import star.random;
import star.byte_array;
import star.secure_random;

namespace Star {

void RandomSource::init(RandomSource& source) {
  init(source.randu64());
}

RandomSource::RandomSource(std::uint64_t seed) {
  init(seed);
}

void RandomSource::init(std::uint64_t seed) {
  /* choose random initial m_carry < 809430660 and */
  /* 256 random 32-bit integers for m_data[]    */
  m_carry = seed % 809430660;

  m_data[0] = seed;
  m_data[1] = seed >> 32;

  for (std::size_t i = 2; i < 256; ++i)
    m_data[i] = 69069 * m_data[i - 2] + 362437;

  m_index = 255;

  // Hard-coded initial skip of random values, to get the random generator
  // going.
  const unsigned RandomInitialSkip = 32;
  for (unsigned i = 0; i < RandomInitialSkip; ++i)
    gen32();
}

void RandomSource::addEntropy(RandomSource& source) {
  addEntropy(source.randu64());
}

void RandomSource::addEntropy(std::uint64_t seed) {
  // to avoid seed aliasing
  seed ^= randu64();

  // Same algo as init, but bitwise xor with existing data

  m_carry = (m_carry ^ seed) % 809430660;

  m_data[0] ^= seed;
  m_data[1] ^= (seed >> 32) ^ seed;

  for (std::size_t i = 2; i < 256; ++i)
    m_data[i] ^= 69069 * m_data[i - 2] + 362437;
}

std::uint32_t RandomSource::randu32() {
  return gen32();
}

std::uint64_t RandomSource::randu64() {
  std::uint64_t r = randu32();
  r = r << 32;
  r = r | randu32();
  return r;
}

std::int32_t RandomSource::randi32() { return static_cast<std::int32_t>((randu32())); }

std::int64_t RandomSource::randi64() { return static_cast<std::int64_t>((randu64())); }

float RandomSource::randf() {
  return (randu32() & 0x7fffffff) / 2147483648.0f;
}

double RandomSource::randd() {
  return (randu64() & 0x7fffffffffffffff) / 9223372036854775808.0;
}

std::int64_t RandomSource::randInt(std::int64_t max) {
  return randUInt(max);
}

std::uint64_t RandomSource::randUInt(std::uint64_t max) {
  std::uint64_t denom = static_cast<std::uint64_t>((-1)) / (static_cast<std::uint64_t>(max) + 1);
  return randu64() / denom;
}

std::expected<std::int64_t, StarException> RandomSource::randInt(std::int64_t min, std::int64_t max) {
  if (max < min)
    return std::unexpected(StarException("Maximum bound in randInt must be >= minimum bound!"));
  return randInt(max - min) + min;
}

std::expected<std::uint64_t, StarException> RandomSource::randUInt(std::uint64_t min, std::uint64_t max) {
  if (max < min)
    return std::unexpected(StarException("Maximum bound in randUInt must be >= minimum bound!"));
  return randUInt(max - min) + min;
}

std::expected<float, StarException> RandomSource::randf(float min, float max) {
  if (max < min)
    return std::unexpected(StarException("Maximum bound in randf must be >= minimum bound!"));
  return randf() * (max - min) + min;
}

std::expected<double, StarException> RandomSource::randd(double min, double max) {
  if (max < min)
    return std::unexpected(StarException("Maximum bound in randd must be >= minimum bound!"));
  return randd() * (max - min) + min;
}

bool RandomSource::randb() {
  return (std::popcount(gen32()) & 1) != 0;
}

void RandomSource::randBytes(std::span<char> bytes) {
  std::size_t offset = 0;
  while (offset < bytes.size()) {
    std::uint32_t ui = gen32();
    for (std::size_t i = 0; i < 4 && offset < bytes.size(); ++i)
      bytes[offset++] = static_cast<char>((ui >> (i * 8)));
  }
}

ByteArray RandomSource::randBytes(std::size_t len) {
  ByteArray array(len, 0);
  randBytes(array.span());
  return array;
}

// normal distribution via Box-Muller
float RandomSource::nrandf(float stddev, float mean) {
  float rand1, rand2, distSqr;
  do {
    rand1 = 2 * randf() - 1;
    rand2 = 2 * randf() - 1;
    distSqr = rand1 * rand1 + rand2 * rand2;
  } while (distSqr >= 1);

  float mapping = std::sqrt(-2 * std::log(distSqr) / distSqr);
  return (rand1 * mapping * stddev + mean);
}

double RandomSource::nrandd(double stddev, double mean) {
  double rand1, rand2, distSqr;
  do {
    rand1 = 2 * randd() - 1;
    rand2 = 2 * randd() - 1;
    distSqr = rand1 * rand1 + rand2 * rand2;
  } while (distSqr >= 1);

  double mapping = std::sqrt(-2 * std::log(distSqr) / distSqr);
  return (rand1 * mapping * stddev + mean);
}

std::int64_t RandomSource::stochasticRound(double val) {
  double fpart = val - std::floor(val);
  if (randd() < fpart)
    return std::ceil(val);
  else
    return std::floor(val);
}

std::uint32_t RandomSource::gen32() {
  std::uint64_t a = 809430660;
  std::uint64_t t = a * m_data[++m_index] + m_carry;

  m_carry = (t >> 32);
  m_data[m_index] = t;

  return t;
}

namespace {
std::expected<std::uint64_t, StarException> produceRandomSeed() {
  auto entropy = secureRandomBytes(sizeof(std::uint64_t));
  if (!entropy)
    return std::unexpected(std::move(entropy.error()));
  std::uint64_t random;
  std::memcpy(&random, entropy->ptr(), sizeof(random));
  return static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count())
      * 1099511628211ULL ^ random;
}
}

std::expected<std::unique_ptr<RandomService>, StarException> RandomService::create() {
  auto seed = produceRandomSeed();
  if (!seed)
    return std::unexpected(std::move(seed.error()));
  return std::make_unique<RandomService>(*seed);
}
RandomService::RandomService(std::uint64_t seed) : m_source(seed) {}

std::expected<void, StarException> RandomService::init() {
  auto seed = produceRandomSeed();
  if (!seed)
    return std::unexpected(std::move(seed.error()));
  init(*seed);
  return {};
}

void RandomService::init(std::uint64_t seed) {
  std::lock_guard locker(m_mutex);
  m_source.init(seed);
}

std::expected<void, StarException> RandomService::addEntropy() {
  auto seed = produceRandomSeed();
  if (!seed)
    return std::unexpected(std::move(seed.error()));
  addEntropy(*seed);
  return {};
}

void RandomService::addEntropy(std::uint64_t seed) {
  std::lock_guard locker(m_mutex);
  m_source.addEntropy(seed);
}

#define STAR_RANDOM_METHOD(Type, Name, Parameters, Arguments) \
  Type RandomService::Name Parameters { \
    std::lock_guard locker(m_mutex); \
    return m_source.Name Arguments; \
  }

STAR_RANDOM_METHOD(std::uint32_t, randu32, (), ())
STAR_RANDOM_METHOD(std::uint64_t, randu64, (), ())
STAR_RANDOM_METHOD(std::int32_t, randi32, (), ())
STAR_RANDOM_METHOD(std::int64_t, randi64, (), ())
STAR_RANDOM_METHOD(float, randf, (), ())
STAR_RANDOM_METHOD(double, randd, (), ())
STAR_RANDOM_METHOD(bool, randb, (), ())
STAR_RANDOM_METHOD(long long, randInt, (long long max), (max))
STAR_RANDOM_METHOD(unsigned long long, randUInt, (unsigned long long max), (max))
std::expected<long long, StarException> RandomService::randInt(long long min, long long max) {
  std::lock_guard locker(m_mutex);
  return m_source.randInt(min, max);
}

std::expected<unsigned long long, StarException> RandomService::randUInt(unsigned long long min, unsigned long long max) {
  std::lock_guard locker(m_mutex);
  return m_source.randUInt(min, max);
}

std::expected<float, StarException> RandomService::randf(float min, float max) {
  std::lock_guard locker(m_mutex);
  return m_source.randf(min, max);
}

std::expected<double, StarException> RandomService::randd(double min, double max) {
  std::lock_guard locker(m_mutex);
  return m_source.randd(min, max);
}
STAR_RANDOM_METHOD(float, nrandf, (float stddev, float mean), (stddev, mean))
STAR_RANDOM_METHOD(double, nrandd, (double stddev, double mean), (stddev, mean))
STAR_RANDOM_METHOD(std::int64_t, stochasticRound, (double val), (val))
STAR_RANDOM_METHOD(void, randBytes, (std::span<char> bytes), (bytes))
STAR_RANDOM_METHOD(ByteArray, randBytes, (std::size_t len), (len))

#undef STAR_RANDOM_METHOD

}
