import std;
import star.format;
import star.math_common;
import star.random;
import star.byte_array;
import star.secure_random;

namespace Star {

RandomSource::RandomSource(std::uint64_t value) {
  seed(value);
}

void RandomSource::seed(RandomSource& source) {
  seed(source.next_u64());
}

void RandomSource::seed(std::uint64_t value) {
  m_carry = value % 809430660;
  m_data[0] = value;
  m_data[1] = value >> 32;
  for (std::size_t i = 2; i < m_data.size(); ++i)
    m_data[i] = 69069 * m_data[i - 2] + 362437;
  m_index = 255;
  for (unsigned i = 0; i < 32; ++i)
    generate_u32();
}

void RandomSource::mix_seed(RandomSource& source) {
  mix_seed(source.next_u64());
}

void RandomSource::mix_seed(std::uint64_t value) {
  value ^= next_u64();
  m_carry = (m_carry ^ value) % 809430660;
  m_data[0] ^= value;
  m_data[1] ^= (value >> 32) ^ value;
  for (std::size_t i = 2; i < m_data.size(); ++i)
    m_data[i] ^= 69069 * m_data[i - 2] + 362437;
}

std::int32_t RandomSource::next_i32() { return std::bit_cast<std::int32_t>(next_u32()); }
std::int64_t RandomSource::next_i64() { return std::bit_cast<std::int64_t>(next_u64()); }

bool RandomSource::next_bool() {
  return (std::popcount(generate_u32()) & 1) != 0;
}

void RandomSource::fill_bytes(std::span<char> bytes) {
  while (bytes.size() >= 4) {
    auto word = generate_u32();
    bytes[0] = static_cast<char>(word);
    bytes[1] = static_cast<char>(word >> 8);
    bytes[2] = static_cast<char>(word >> 16);
    bytes[3] = static_cast<char>(word >> 24);
    bytes = bytes.subspan(4);
  }
  if (!bytes.empty()) {
    auto word = generate_u32();
    for (auto& byte : bytes) {
      byte = static_cast<char>(word);
      word >>= 8;
    }
  }
}

ByteArray RandomSource::random_bytes(std::size_t size) {
  ByteArray bytes;
  bytes.resize(size);
  fill_bytes(bytes.span());
  return bytes;
}

void RandomSource::fill_uniform(std::span<float> values) {
  for (auto& value : values)
    value = uniform_float();
}

std::expected<std::int64_t, StarException> RandomSource::stochastic_round(double value) {
  // INT64_MAX rounds to 2^63 as a double, so use an exclusive upper bound.
  if (!is_finite(value) || value < -0x1p63 || value >= 0x1p63)
    return std::unexpected(RandomException("Stochastic rounding value is outside int64 range"));
  auto floor = std::floor(value);
  auto fraction = value - floor;
  return static_cast<std::int64_t>(uniform_double() < fraction ? std::ceil(value) : floor);
}

namespace {
std::expected<std::uint64_t, StarException> produce_random_seed() {
  std::array<char, sizeof(std::uint64_t)> bytes;
  auto filled = secure_random_fill(bytes);
  if (!filled)
    return std::unexpected(std::move(filled.error()));
  auto random = std::bit_cast<std::uint64_t>(bytes);
  return static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count())
      * 1099511628211ULL ^ random;
}
}

std::expected<std::unique_ptr<RandomService>, StarException> RandomService::create() {
  auto value = produce_random_seed();
  if (!value)
    return std::unexpected(std::move(value.error()));
  return std::make_unique<RandomService>(*value);
}
RandomService::RandomService(std::uint64_t value) : m_source(value) {}

std::expected<void, StarException> RandomService::seed() {
  auto value = produce_random_seed();
  if (!value)
    return std::unexpected(std::move(value.error()));
  seed(*value);
  return {};
}

void RandomService::seed(std::uint64_t value) {
  std::lock_guard lock(m_mutex);
  m_source.seed(value);
}

std::expected<void, StarException> RandomService::mix_seed() {
  auto value = produce_random_seed();
  if (!value)
    return std::unexpected(std::move(value.error()));
  mix_seed(*value);
  return {};
}

void RandomService::mix_seed(std::uint64_t value) {
  std::lock_guard lock(m_mutex);
  m_source.mix_seed(value);
}

#define STAR_RANDOM_METHOD(Type, Name, Parameters, Arguments) \
  Type RandomService::Name Parameters { \
    std::lock_guard lock(m_mutex); \
    return m_source.Name Arguments; \
  }

STAR_RANDOM_METHOD(std::uint32_t, next_u32, (), ())
STAR_RANDOM_METHOD(std::uint64_t, next_u64, (), ())
STAR_RANDOM_METHOD(std::int32_t, next_i32, (), ())
STAR_RANDOM_METHOD(std::int64_t, next_i64, (), ())
STAR_RANDOM_METHOD(float, uniform_float, (), ())
STAR_RANDOM_METHOD(double, uniform_double, (), ())
STAR_RANDOM_METHOD(bool, next_bool, (), ())
STAR_RANDOM_METHOD(std::uint64_t, uniform_uint, (std::uint64_t max), (max))
STAR_RANDOM_METHOD(void, fill_bytes, (std::span<char> bytes), (bytes))
STAR_RANDOM_METHOD(ByteArray, random_bytes, (std::size_t size), (size))
STAR_RANDOM_METHOD(void, fill_uniform, (std::span<float> values), (values))

#undef STAR_RANDOM_METHOD

std::expected<std::int64_t, StarException> RandomService::uniform_int(std::int64_t max) {
  std::lock_guard lock(m_mutex);
  return m_source.uniform_int(max);
}
std::expected<std::int64_t, StarException> RandomService::uniform_int(std::int64_t min, std::int64_t max) {
  std::lock_guard lock(m_mutex);
  return m_source.uniform_int(min, max);
}
std::expected<std::uint64_t, StarException> RandomService::uniform_uint(std::uint64_t min, std::uint64_t max) {
  std::lock_guard lock(m_mutex);
  return m_source.uniform_uint(min, max);
}
std::expected<float, StarException> RandomService::uniform_float(float min, float max) {
  std::lock_guard lock(m_mutex);
  return m_source.uniform_float(min, max);
}
std::expected<double, StarException> RandomService::uniform_double(double min, double max) {
  std::lock_guard lock(m_mutex);
  return m_source.uniform_double(min, max);
}
std::expected<float, StarException> RandomService::normal_float(float stddev, float mean) {
  std::lock_guard lock(m_mutex);
  return m_source.normal_float(stddev, mean);
}
std::expected<double, StarException> RandomService::normal_double(double stddev, double mean) {
  std::lock_guard lock(m_mutex);
  return m_source.normal_double(stddev, mean);
}
std::expected<std::int64_t, StarException> RandomService::stochastic_round(double value) {
  std::lock_guard lock(m_mutex);
  return m_source.stochastic_round(value);
}

}
