module;
#include <stddef.h>
#include "curve25519/include/curve25519_dh.h"
#include "curve25519/include/ed25519_signature.h"
import std;
import star.random;


// The native C declarations are parsed in the global module fragment.
import star.string;
import star.byte_array;
import star.encode;
import star.array;

export module star.curve25519;

export namespace Star {
namespace Curve25519 {

constexpr std::size_t PublicKeySize = 32;
constexpr std::size_t SecretKeySize = 32;
constexpr std::size_t PrivateKeySize = 64;
constexpr std::size_t SignatureSize = 64;

using PublicKey = Array<std::uint8_t, PublicKeySize>;
using SecretKey = Array<std::uint8_t, SecretKeySize>;
using PrivateKey = Array<std::uint8_t, PrivateKeySize>;
using Signature = Array<std::uint8_t, SignatureSize>;

// The process/application owner chooses the identity lifetime explicitly.
// Generation remains lazy, so owning an identity does not consume its injected
// random stream until the first publication or signature. The source outlives
// this object.
class KeySet {
public:
  explicit KeySet(RandomService& random);
  explicit KeySet(RandomSource& random);

  PublicKey const& publicKey() const;
  Signature sign(std::span<char const> data) const;

private:
  void generate() const;

  std::variant<std::reference_wrapper<RandomService>, std::reference_wrapper<RandomSource>> m_random;
  mutable std::once_flag m_generated;
  mutable PrivateKey m_privateKey;
  mutable PublicKey m_publicKey;
};

bool verify(std::span<std::uint8_t const, SignatureSize> signature, std::span<std::uint8_t const, PublicKeySize> publicKey, std::span<char const> data);


}
}

// Keep the native crypto definitions in the implementation portion.
namespace Star {
namespace Curve25519 {
KeySet::KeySet(RandomService& random) : m_random(std::ref(random)) {}

KeySet::KeySet(RandomSource& random) : m_random(std::ref(random)) {}

void KeySet::generate() const {
  std::call_once(m_generated, [this] {
    SecretKey secret;
    std::visit([&secret](auto random) {
      random.get().randBytes({reinterpret_cast<char*>(secret.data()), secret.size()});
    }, m_random);
    secret[0]  &= 248;
    secret[31] &= 127;
    secret[31] |= 64;

    ed25519_CreateKeyPair(m_publicKey.data(), m_privateKey.data(), nullptr, secret.data());
  });
}

PublicKey const& KeySet::publicKey() const {
  generate();
  return m_publicKey;
}

Signature KeySet::sign(std::span<char const> data) const {
  generate();
  Signature signature;
  ed25519_SignMessage(signature.data(), m_privateKey.data(), nullptr, reinterpret_cast<unsigned char const*>(data.data()), data.size());
  return signature;
}

bool verify(std::span<std::uint8_t const, SignatureSize> signature, std::span<std::uint8_t const, PublicKeySize> publicKey, std::span<char const> data) {
  return ed25519_VerifySignature(signature.data(), publicKey.data(), reinterpret_cast<unsigned char const*>(data.data()), data.size());
}


}
}