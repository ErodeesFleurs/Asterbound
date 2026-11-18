module;
import std;
import star.format;

export module star.curve25519;

export namespace Star::Curve25519 {

inline constexpr std::size_t public_key_size = 32;
inline constexpr std::size_t secret_key_size = 32;
inline constexpr std::size_t signature_size = 64;

using PublicKey = std::array<std::uint8_t, public_key_size>;
using SecretKey = std::array<std::uint8_t, secret_key_size>;
using Signature = std::array<std::uint8_t, signature_size>;

// An initialized application identity, with exclusive ownership of its secret.
class KeySet {
public:
  [[nodiscard]] static std::expected<KeySet, StarException> create();
  [[nodiscard]] static std::expected<KeySet, StarException> from_seed(
      std::span<std::uint8_t const, secret_key_size> seed);

  KeySet(KeySet const&) = delete;
  KeySet& operator=(KeySet const&) = delete;
  KeySet(KeySet&& other) noexcept;
  KeySet& operator=(KeySet&& other) noexcept;
  ~KeySet();

  PublicKey const& public_key() const& noexcept;
  PublicKey const& public_key() const&& = delete;
  Signature sign(std::span<char const> message) const;

private:
  KeySet() = default;

  PublicKey m_public_key_bytes{};
  // libsodium's signing key is the seed followed by its public key.
  std::array<std::uint8_t, secret_key_size + public_key_size> m_private_key_bytes{};
  bool m_initialized = false;
};

// Reuses validated public-key bytes, not an undocumented native point cache.
class Verifier {
public:
  [[nodiscard]] static std::expected<Verifier, StarException> create(
      std::span<std::uint8_t const> public_key);
  [[nodiscard]] bool verify(std::span<std::uint8_t const> signature,
      std::span<char const> message) const;
  [[nodiscard]] bool verify(std::span<char const> signature,
      std::span<char const> message) const;

private:
  explicit Verifier(std::span<std::uint8_t const, public_key_size> public_key) noexcept;

  PublicKey m_public_key_bytes;
};

[[nodiscard]] bool verify(std::span<std::uint8_t const> signature,
    std::span<std::uint8_t const> public_key, std::span<char const> message);

}