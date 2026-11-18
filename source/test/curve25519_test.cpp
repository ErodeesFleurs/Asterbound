#ifdef STAR_TEST_WRAP_READ
#include <cerrno>
#include <unistd.h>
#endif
import std;
import star.curve25519;
import star.encode;
import star.byte_array;
import star.format;
#include "gtest/gtest.h"

using namespace Star;
namespace {
using namespace Star::Curve25519;

template <std::size_t extent>
std::array<std::uint8_t, extent> fixed_hex(std::string_view text) {
  if (text.size() != extent * 2)
    fatal_error("Invalid Ed25519 test hex length", false);
  auto bytes = hex_decode(text);
  if (bytes.size() != extent)
    fatal_error("Invalid Ed25519 test vector", false);
  std::array<std::uint8_t, extent> result;
  std::ranges::copy(bytes, result.begin());
  return result;
}

constexpr std::string_view seed_hex = "9d61b19deffd5a60ba844af492ec2cc4" "4449c5697b326919703bac031cae7f60";
constexpr std::string_view public_hex = "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a";
constexpr std::string_view signature_hex =
    "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e06522490155"
    "5fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b";

#ifdef STAR_TEST_WRAP_READ
thread_local bool fail_entropy_read = false;
#endif
}

#ifdef STAR_TEST_WRAP_READ
extern "C" ssize_t __real_read(int, void*, size_t);
extern "C" ssize_t __wrap_read(int descriptor, void* output, size_t size) {
  if (std::exchange(fail_entropy_read, false)) {
    errno = EIO;
    return -1;
  }
  return __real_read(descriptor, output, size);
}
#endif

TEST(Curve25519Test, StandardSeedAndEmptyMessageMatchRfc8032) {
  auto key = Curve25519::KeySet::from_seed(fixed_hex<32>(seed_hex));
  ASSERT_TRUE(key) << key.error().what();
  auto expected_public = fixed_hex<32>(public_hex);
  auto expected_signature = fixed_hex<64>(signature_hex);
  EXPECT_EQ(key->public_key(), expected_public);
  EXPECT_EQ(key->sign({}), expected_signature);
  EXPECT_TRUE(Curve25519::verify(expected_signature, expected_public, {}));
  auto verifier = Curve25519::Verifier::create(expected_public);
  ASSERT_TRUE(verifier) << verifier.error().what();
  EXPECT_TRUE(verifier->verify(expected_signature, {}));
  EXPECT_FALSE(verifier->verify(expected_signature, std::span<char const>("x", 1)));
}

TEST(Curve25519Test, OwnershipTransfersPreserveSigningIdentity) {
  static_assert(!std::copy_constructible<Curve25519::KeySet>);
  static_assert(std::is_nothrow_move_constructible_v<Curve25519::KeySet>);
  auto first = Curve25519::KeySet::from_seed(fixed_hex<32>(seed_hex));
  auto second = Curve25519::KeySet::from_seed(SecretKey{});
  ASSERT_TRUE(first);
  ASSERT_TRUE(second);
  auto moved = std::move(*first);
  *second = std::move(moved);
  EXPECT_EQ(second->public_key(), fixed_hex<32>(public_hex));
  EXPECT_EQ(second->sign({}), fixed_hex<64>(signature_hex));
}

TEST(Curve25519Test, ArbitraryMessageBytesAndRepeatedVerificationRetainBoundaries) {
  auto key = Curve25519::KeySet::from_seed(fixed_hex<32>(seed_hex));
  ASSERT_TRUE(key);
  auto verifier = Curve25519::Verifier::create(key->public_key());
  ASSERT_TRUE(verifier);
  std::array<char, 4096> message;
  for (std::size_t index = 0; index < message.size(); ++index)
    message[index] = static_cast<char>(index);
  for (auto size : {1u, 64u, 1024u, 4096u}) {
    auto bytes = std::span<char const>(message).first(size);
    auto signature = key->sign(bytes);
    EXPECT_TRUE(verifier->verify(signature, bytes));
    message[size - 1] ^= 1;
    EXPECT_FALSE(verifier->verify(signature, bytes));
    message[size - 1] ^= 1;
    EXPECT_TRUE(verifier->verify(signature, bytes));
  }
}

TEST(Curve25519Test, ShortAndOversizedInputsFailBeforeFixedExtentReads) {
  auto public_key = fixed_hex<32>(public_hex);
  auto signature = fixed_hex<64>(signature_hex);
  auto verifier = Curve25519::Verifier::create(public_key);
  ASSERT_TRUE(verifier);
  for (std::size_t size = 0; size < public_key.size(); ++size) {
    EXPECT_FALSE(Curve25519::verify(signature, std::span(public_key).first(size), {}));
    EXPECT_FALSE(Curve25519::Verifier::create(std::span(public_key).first(size)));
  }
  for (std::size_t size = 0; size < signature.size(); ++size) {
    EXPECT_FALSE(Curve25519::verify(std::span(signature).first(size), public_key, {}));
    EXPECT_FALSE(verifier->verify(std::span(signature).first(size), {}));
  }
  std::array<std::uint8_t, 65> oversized_signature{};
  std::array<std::uint8_t, 33> oversized_key{};
  EXPECT_FALSE(Curve25519::verify(oversized_signature, public_key, {}));
  EXPECT_FALSE(verifier->verify(oversized_signature, {}));
  EXPECT_FALSE(Curve25519::Verifier::create(oversized_key));
}

TEST(Curve25519Test, RejectsMalleableScalarAndIdentityKeyForgery) {
  auto public_key = fixed_hex<32>(public_hex);
  auto signature = fixed_hex<64>(signature_hex);
  auto order = fixed_hex<32>("edd3f55c1a631258d69cf7a2def9de1400000000000000000000000000000010");
  unsigned carry = 0;
  for (std::size_t index = 0; index < order.size(); ++index) {
    unsigned sum = signature[32 + index] + order[index] + carry;
    signature[32 + index] = static_cast<std::uint8_t>(sum);
    carry = sum >> 8;
  }
  EXPECT_FALSE(Curve25519::verify(signature, public_key, {}));
  auto verifier = Curve25519::Verifier::create(public_key);
  ASSERT_TRUE(verifier);
  EXPECT_FALSE(verifier->verify(signature, {}));
  PublicKey identity{};
  identity[0] = 1;
  Signature forged{};
  auto base_point = fixed_hex<32>("5866666666666666666666666666666666666666666666666666666666666666");
  std::ranges::copy(base_point, forged.begin());
  forged[32] = 1;
  EXPECT_FALSE(Curve25519::Verifier::create(identity));
  EXPECT_FALSE(Curve25519::verify(forged, identity, {}));
  EXPECT_FALSE(Curve25519::Verifier::create(fixed_hex<32>("edffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f")));
}

#ifdef STAR_TEST_WRAP_READ
TEST(Curve25519Test, EntropyFailureCannotCreateAnIdentity) {
  // Finish native initialization before the scoped OS-read failure.
  ASSERT_TRUE(Curve25519::KeySet::from_seed(SecretKey{}));
  fail_entropy_read = true;
  auto generated = Curve25519::KeySet::create();
  fail_entropy_read = false;
  EXPECT_FALSE(generated);
  auto recovered = Curve25519::KeySet::create();
  ASSERT_TRUE(recovered) << recovered.error().what();
  auto signature = recovered->sign({});
  EXPECT_TRUE(Curve25519::verify(signature, recovered->public_key(), {}));
}
#endif

TEST(Curve25519Test, NonemptyRfc8032VectorRetainsPureEd25519Encoding) {
  auto seed = fixed_hex<32>("833fe62409237b9d62ec77587520911e9a759cec1d19755b7da901b96dca3d42");
  auto public_key = fixed_hex<32>("ec172b93ad5e563bf4932c70e1245034c35467ef2efd4d64ebf819683467e2bf");
  auto message = hex_decode(
      "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
      "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f");
  auto signature = fixed_hex<64>(
      "dc2a4459e7369633a52b1bf277839a00201009a3efbf3ecb69bea2186c26b589"
      "09351fc9ac90b3ecfdfbc7c66431e0303dca179c138ac17ad9bef1177331a704");
  auto key = Curve25519::KeySet::from_seed(seed);
  ASSERT_TRUE(key);
  EXPECT_EQ(key->public_key(), public_key);
  EXPECT_EQ(key->sign(message.span()), signature);
  EXPECT_TRUE(Curve25519::verify(signature, public_key, message.span()));
}
