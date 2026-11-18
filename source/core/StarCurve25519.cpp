module;
#include <sodium.h>
import std;
import star.format;
import star.secure_random;

module star.curve25519;

namespace Star::Curve25519 {
namespace {

static_assert(public_key_size == crypto_sign_PUBLICKEYBYTES);
static_assert(secret_key_size == crypto_sign_SEEDBYTES);
static_assert(signature_size == crypto_sign_BYTES);
static_assert(secret_key_size + public_key_size == crypto_sign_SECRETKEYBYTES);
static_assert(SODIUM_LIBRARY_VERSION_MAJOR > 26
    || (SODIUM_LIBRARY_VERSION_MAJOR == 26 && SODIUM_LIBRARY_VERSION_MINOR >= 3),
    "Ed25519 subgroup validation requires libsodium 1.0.21 or newer.");

bool sodium_ready() noexcept {
  // The same SONAME can cover older releases with incomplete subgroup checks.
  static bool const ready = [] {
    auto major = sodium_library_version_major();
    auto minor = sodium_library_version_minor();
    return (major > 26 || (major == 26 && minor >= 3)) && sodium_init() >= 0;
  }();
  return ready;
}

struct SeedBuffer {
  SecretKey bytes{};

  SeedBuffer() = default;
  SeedBuffer(SeedBuffer const&) = delete;
  SeedBuffer& operator=(SeedBuffer const&) = delete;
  ~SeedBuffer() {
    sodium_memzero(bytes.data(), bytes.size());
  }
};

template <class Byte>
concept signature_byte = std::same_as<Byte, char> || std::same_as<Byte, std::uint8_t>;

template <signature_byte Byte>
bool verify_signature(std::span<Byte const> signature, PublicKey const& public_key,
    std::span<char const> message) {
  if (signature.size() != signature_size)
    return false;
  // Only the native boundary changes byte representation; no payload copies.
  if (crypto_core_ed25519_is_valid_point(
          reinterpret_cast<unsigned char const*>(signature.data())) != 1)
    return false;
  return crypto_sign_verify_detached(reinterpret_cast<unsigned char const*>(signature.data()),
      reinterpret_cast<unsigned char const*>(message.data()), message.size(), public_key.data()) == 0;
}

}

std::expected<KeySet, StarException> KeySet::create() {
  if (!sodium_ready())
    return std::unexpected(StarException("Could not initialize libsodium 1.0.21 or newer."));
  SeedBuffer seed;
  auto filled = secure_random_fill({reinterpret_cast<char*>(seed.bytes.data()), seed.bytes.size()});
  if (!filled)
    return std::unexpected(std::move(filled.error()));
  return from_seed(seed.bytes);
}

std::expected<KeySet, StarException> KeySet::from_seed(
    std::span<std::uint8_t const, secret_key_size> seed) {
  if (!sodium_ready())
    return std::unexpected(StarException("Could not initialize libsodium 1.0.21 or newer."));
  KeySet key_set;
  if (crypto_sign_seed_keypair(key_set.m_public_key_bytes.data(),
          key_set.m_private_key_bytes.data(), seed.data()) != 0)
    return std::unexpected(StarException("Could not derive Ed25519 identity."));
  key_set.m_initialized = true;
  return key_set;
}

KeySet::KeySet(KeySet&& other) noexcept
  : m_public_key_bytes(other.m_public_key_bytes), m_private_key_bytes(other.m_private_key_bytes),
    m_initialized(std::exchange(other.m_initialized, false)) {
  sodium_memzero(other.m_private_key_bytes.data(), other.m_private_key_bytes.size());
  other.m_public_key_bytes.fill(0);
}

KeySet& KeySet::operator=(KeySet&& other) noexcept {
  if (this != &other) {
    sodium_memzero(m_private_key_bytes.data(), m_private_key_bytes.size());
    m_public_key_bytes = other.m_public_key_bytes;
    m_private_key_bytes = other.m_private_key_bytes;
    m_initialized = std::exchange(other.m_initialized, false);
    sodium_memzero(other.m_private_key_bytes.data(), other.m_private_key_bytes.size());
    other.m_public_key_bytes.fill(0);
  }
  return *this;
}

KeySet::~KeySet() {
  sodium_memzero(m_private_key_bytes.data(), m_private_key_bytes.size());
}

PublicKey const& KeySet::public_key() const& noexcept {
  if (!m_initialized)
    std::terminate();
  return m_public_key_bytes;
}

Signature KeySet::sign(std::span<char const> message) const {
  if (!m_initialized)
    std::terminate();
  Signature signature;
  // Only the C ABI boundary adapts byte spans to native pointer arguments.
  if (crypto_sign_detached(signature.data(), nullptr,
          reinterpret_cast<unsigned char const*>(message.data()), message.size(),
          m_private_key_bytes.data()) != 0)
    std::terminate();
  return signature;
}

Verifier::Verifier(std::span<std::uint8_t const, public_key_size> public_key) noexcept {
  std::ranges::copy(public_key, m_public_key_bytes.begin());
}

std::expected<Verifier, StarException> Verifier::create(std::span<std::uint8_t const> public_key) {
  if (public_key.size() != public_key_size)
    return std::unexpected(StarException("Ed25519 public key must contain 32 bytes."));
  if (!sodium_ready())
    return std::unexpected(StarException("Could not initialize libsodium 1.0.21 or newer."));
  if (crypto_core_ed25519_is_valid_point(public_key.data()) != 1)
    return std::unexpected(StarException("Ed25519 public key is not a canonical main-subgroup point."));
  return Verifier(std::span<std::uint8_t const, public_key_size>(public_key));
}

bool Verifier::verify(std::span<std::uint8_t const> signature, std::span<char const> message) const {
  return verify_signature(signature, m_public_key_bytes, message);
}

bool Verifier::verify(std::span<char const> signature, std::span<char const> message) const {
  return verify_signature(signature, m_public_key_bytes, message);
}

bool verify(std::span<std::uint8_t const> signature, std::span<std::uint8_t const> public_key,
    std::span<char const> message) {
  if (signature.size() != signature_size || public_key.size() != public_key_size)
    return false;
  if (!sodium_ready())
    return false;
  if (crypto_core_ed25519_is_valid_point(public_key.data()) != 1
      || crypto_core_ed25519_is_valid_point(signature.data()) != 1)
    return false;
  return crypto_sign_verify_detached(signature.data(),
      reinterpret_cast<unsigned char const*>(message.data()), message.size(), public_key.data()) == 0;
}

}
