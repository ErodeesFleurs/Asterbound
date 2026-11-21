module;
import star.string;
import star.byte_array;
import std;

export module star.sha256;

export namespace Star {

class Sha256Hasher {
public:
  Sha256Hasher() = default;

  // After compute(), any push (even an empty one) starts a new message.
  void push(std::span<char const> data);
  void push(String const& data);
  void push(ByteArray const& data);

  // Finalizes the digest once; subsequent reads produce the same 32 bytes.
  void compute(std::span<char, 32> destination);
  ByteArray compute();

private:
  struct State {
    static constexpr std::array<std::uint32_t, 8> initial_words = {
        0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
        0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};

    std::array<std::uint32_t, 8> words = initial_words;
    std::array<char, 64> buffer{};
    std::uint64_t bit_length = 0;
    bool finished = false;

    std::size_t buffered_size() const noexcept {
      return static_cast<std::size_t>((bit_length / 8) % buffer.size());
    }
    void compress(std::span<char const, 64> block);
    void finish();
  };

  State m_state;
};

void sha256(std::span<char const> source, std::span<char, 32> destination);
ByteArray sha256(std::span<char const> source);
ByteArray sha256(ByteArray const& in);
ByteArray sha256(String const& in);

}

namespace Star {

// SHA-256 compression, adapted from Tom St Denis's implementation.
static constexpr std::array<std::uint32_t, 64> round_constants = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};

static constexpr std::uint32_t sha_choice(std::uint32_t x, std::uint32_t y, std::uint32_t z) noexcept {
  return (x & y) ^ (~x & z);
}

static constexpr std::uint32_t sha_majority(std::uint32_t x, std::uint32_t y, std::uint32_t z) noexcept {
  return (x & y) ^ (x & z) ^ (y & z);
}

static constexpr std::uint32_t sha_sigma_0(std::uint32_t x) noexcept {
  return std::rotr(x, 2) ^ std::rotr(x, 13) ^ std::rotr(x, 22);
}

static constexpr std::uint32_t sha_sigma_1(std::uint32_t x) noexcept {
  return std::rotr(x, 6) ^ std::rotr(x, 11) ^ std::rotr(x, 25);
}

static constexpr std::uint32_t sha_gamma_0(std::uint32_t x) noexcept {
  return std::rotr(x, 7) ^ std::rotr(x, 18) ^ (x >> 3);
}

static constexpr std::uint32_t sha_gamma_1(std::uint32_t x) noexcept {
  return std::rotr(x, 17) ^ std::rotr(x, 19) ^ (x >> 10);
}

void Sha256Hasher::State::compress(std::span<char const, 64> block) {
  auto bytes = std::as_bytes(block);
  std::array<std::uint32_t, 64> schedule;
  for (std::size_t i = 0; i < 16; ++i) {
    auto offset = 4 * i;
    schedule[i] = (std::to_integer<std::uint32_t>(bytes[offset]) << 24)
        | (std::to_integer<std::uint32_t>(bytes[offset + 1]) << 16)
        | (std::to_integer<std::uint32_t>(bytes[offset + 2]) << 8)
        | std::to_integer<std::uint32_t>(bytes[offset + 3]);
  }
  for (std::size_t i = 16; i < schedule.size(); ++i)
    schedule[i] = sha_gamma_1(schedule[i - 2]) + schedule[i - 7] + sha_gamma_0(schedule[i - 15]) + schedule[i - 16];

  auto [a, b, c, d, e, f, g, h] = words;
  for (std::size_t i = 0; i < schedule.size(); ++i) {
    auto t1 = h + sha_sigma_1(e) + sha_choice(e, f, g) + round_constants[i] + schedule[i];
    auto t2 = sha_sigma_0(a) + sha_majority(a, b, c);
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }
  words[0] += a;
  words[1] += b;
  words[2] += c;
  words[3] += d;
  words[4] += e;
  words[5] += f;
  words[6] += g;
  words[7] += h;
}

void Sha256Hasher::push(std::span<char const> data) {
  if (m_state.finished) {
    m_state.words = State::initial_words;
    m_state.bit_length = 0;
    m_state.finished = false;
  }
  if (data.empty())
    return;

  auto buffered = m_state.buffered_size();
  m_state.bit_length += std::uint64_t{data.size()} * 8;
  auto buffer = std::span(m_state.buffer);
  if (buffered != 0) {
    auto space = buffer.subspan(buffered);
    auto count = std::min(space.size(), data.size());
    std::ranges::copy(data.first(count), space.begin());
    if (count != space.size())
      return;
    m_state.compress(buffer);
    data = data.subspan(count);
  }
  while (data.size() >= buffer.size()) {
    m_state.compress(data.first<64>());
    data = data.subspan<64>();
  }
  std::ranges::copy(data, buffer.begin());
}

void Sha256Hasher::State::finish() {
  if (finished)
    return;

  auto block = std::span(buffer);
  auto used = buffered_size();
  block[used++] = static_cast<char>(0x80);
  if (used > 56) {
    std::ranges::fill(block.subspan(used), '\0');
    compress(block);
    used = 0;
  }
  std::ranges::fill(block.first<56>().subspan(used), '\0');
  for (std::size_t i = 0; i < 8; ++i)
    block[63 - i] = static_cast<char>((bit_length >> (8 * i)) & 0xffU);
  compress(block);
  finished = true;
}

void Sha256Hasher::push(String const& data) {
  push(std::span<char const>(data.utf8()));
}

void Sha256Hasher::push(ByteArray const& data) {
  push(data.span());
}

ByteArray Sha256Hasher::compute() {
  ByteArray destination;
  destination.resize(32);
  compute(destination.span().first<32>());
  return destination;
}

void Sha256Hasher::compute(std::span<char, 32> destination) {
  m_state.finish();
  for (std::size_t i = 0; i < m_state.words.size(); ++i) {
    auto word = m_state.words[i];
    destination[4 * i] = static_cast<char>(word >> 24);
    destination[4 * i + 1] = static_cast<char>((word >> 16) & 0xffU);
    destination[4 * i + 2] = static_cast<char>((word >> 8) & 0xffU);
    destination[4 * i + 3] = static_cast<char>(word & 0xffU);
  }
}

void sha256(std::span<char const> source, std::span<char, 32> destination) {
  Sha256Hasher hasher;
  hasher.push(source);
  hasher.compute(destination);
}

ByteArray sha256(std::span<char const> source) {
  Sha256Hasher hasher;
  hasher.push(source);
  return hasher.compute();
}

ByteArray sha256(ByteArray const& in) {
  return sha256(in.span());
}

ByteArray sha256(String const& in) {
  return sha256(std::span<char const>(in.utf8()));
}

}

