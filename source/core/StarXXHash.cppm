module;
#define XXH_INLINE_ALL
#include "xxhash.h"
#include "xxh3.h"

import std;
import star.string;
import star.byte_array;
import star.bytes;
namespace Star {

class XXHash32 {
public:
  XXHash32(std::uint32_t seed = 0);

  void push(std::span<char const> data);
  std::uint32_t digest();

private:
  XXH32_state_s state;
};

class XXHash64 {
public:
  XXHash64(std::uint64_t seed = 0);

  void push(std::span<char const> data);
  std::uint64_t digest();

private:
  XXH64_state_s state;
};

class XXHash3 {
public:
  XXHash3();

  void push(std::span<char const> data);
  std::uint64_t digest();

private:
  XXH3_state_s state;
};

std::uint32_t xx_hash32(std::span<char const> source, std::uint32_t seed = 0);
std::uint32_t xx_hash32(ByteArray const& in);
std::uint32_t xx_hash32(String const& in);

std::uint64_t xx_hash64(std::span<char const> source, std::uint64_t seed = 0);
std::uint64_t xx_hash64(ByteArray const& in);
std::uint64_t xx_hash64(String const& in);

std::uint64_t xx_hash3(std::span<char const> source);
std::uint64_t xx_hash3(ByteArray const& in);
std::uint64_t xx_hash3(String const& in);

template <typename Hasher>
concept xx_hash_hasher =
  std::same_as<Hasher, XXHash32> || std::same_as<Hasher, XXHash64> || std::same_as<Hasher, XXHash3>;

template <typename Representation>
concept hash_append_representation = std::same_as<Representation, bool> || std::same_as<Representation, std::int32_t>
  || std::same_as<Representation, std::uint32_t> || std::same_as<Representation, std::int64_t>
  || std::same_as<Representation, std::uint64_t> || std::same_as<Representation, float>
  || std::same_as<Representation, double>;

template <typename Value>
concept hash_append_primitive_value =
  std::same_as<Value, bool> || std::same_as<Value, int> || std::same_as<Value, unsigned int>
  || std::same_as<Value, long> || std::same_as<Value, unsigned long> || std::same_as<Value, long long>
  || std::same_as<Value, unsigned long long> || std::same_as<Value, float> || std::same_as<Value, double>;

// One canonical representation map is shared by streaming and bounded packing.
template <hash_append_primitive_value Value> inline auto hash_encode_primitive(Value const& value) {
  using Representation = std::conditional_t<
    std::same_as<Value, int>, std::int32_t,
    std::conditional_t<std::same_as<Value, unsigned int>, std::uint32_t,
                       std::conditional_t<std::same_as<Value, long> || std::same_as<Value, long long>, std::int64_t,
                                          std::conditional_t<std::same_as<Value, unsigned long>
                                                               || std::same_as<Value, unsigned long long>,
                                                             std::uint64_t, Value>>>>;
  static_assert(hash_append_representation<Representation>);
  return toLittleEndian(static_cast<Representation>(value));
}

template <xx_hash_hasher Hasher, hash_append_primitive_value Value>
static inline void hash_append_primitive(Hasher& hash, Value const& value) {
  auto representation = hash_encode_primitive(value);
  hash.push({reinterpret_cast<char const*>(&representation), sizeof(representation)});
}

template <xx_hash_hasher Hasher, typename Value>
  requires std::same_as<Value, bool>
inline void hash_append(Hasher& hash, Value const& value) {
  hash_append_primitive(hash, value);
}

// Keep ordinary numeric overload resolution, including integer promotions.
// long always uses 64 bits, even on platforms where long itself is 32 bits.
template <xx_hash_hasher Hasher> inline void hash_append(Hasher& hash, int const& value) {
  hash_append_primitive(hash, value);
}

template <xx_hash_hasher Hasher> inline void hash_append(Hasher& hash, long const& value) {
  hash_append_primitive(hash, value);
}

template <xx_hash_hasher Hasher> inline void hash_append(Hasher& hash, long long const& value) {
  hash_append_primitive(hash, value);
}

template <xx_hash_hasher Hasher> inline void hash_append(Hasher& hash, unsigned int const& value) {
  hash_append_primitive(hash, value);
}

template <xx_hash_hasher Hasher> inline void hash_append(Hasher& hash, unsigned long const& value) {
  hash_append_primitive(hash, value);
}

template <xx_hash_hasher Hasher> inline void hash_append(Hasher& hash, unsigned long long const& value) {
  hash_append_primitive(hash, value);
}

template <xx_hash_hasher Hasher> inline void hash_append(Hasher& hash, float const& value) {
  hash_append_primitive(hash, value);
}

template <xx_hash_hasher Hasher> inline void hash_append(Hasher& hash, double const& value) {
  hash_append_primitive(hash, value);
}

template <xx_hash_hasher Hasher> inline void hash_append(Hasher& hash, std::string_view text) {
  hash.push(std::span<char const>(text));
}

template <xx_hash_hasher Hasher, std::size_t N> inline void hash_append(Hasher& hash, char const (&text)[N]) {
  std::string_view view(text, N);
  hash_append(hash, view.substr(0, view.find('\0')));
}

template <xx_hash_hasher Hasher> inline void hash_append(Hasher& hash, std::string const& text) {
  hash_append(hash, std::string_view(text));
}

template <xx_hash_hasher Hasher> inline void hash_append(Hasher& hash, String const& text) {
  // Hash the full byte extent, not checked UTF-8 code-point counts.
  hash.push(std::span<char const>(text.utf8()));
}

// XXH3 historically accepted C-string pointer variables through string_view.
// Its new std::string overload must not make those calls ambiguous; arrays
// still take the bounded adapter above. XXH32/64 retain their existing domain.
template <typename Text>
  requires(std::same_as<std::remove_cv_t<Text>, char*> || std::same_as<std::remove_cv_t<Text>, char const*>)
inline void hash_append(XXHash3& hash, Text const& text) {
  hash_append(hash, std::string_view(text));
}

// ADL customizations may return void or an arbitrary temporary whose
// destructor completes the append operation.
template <typename Hasher, typename Value>
concept hash_appendable = requires(Hasher& hash, Value const& value) { hash_append(hash, value); };

inline XXHash32::XXHash32(std::uint32_t seed) {
  XXH32_reset(&state, seed);
}

inline void XXHash32::push(std::span<char const> data) {
  XXH32_update(&state, data.data(), data.size());
}

inline std::uint32_t XXHash32::digest() {
  return XXH32_digest(&state);
}

inline XXHash64::XXHash64(std::uint64_t seed) {
  XXH64_reset(&state, seed);
}

inline void XXHash64::push(std::span<char const> data) {
  XXH64_update(&state, data.data(), data.size());
}

inline std::uint64_t XXHash64::digest() {
  return XXH64_digest(&state);
}

inline XXHash3::XXHash3() {
  XXH3_64bits_reset(&state);
}

inline void XXHash3::push(std::span<char const> data) {
  XXH3_64bits_update(&state, data.data(), data.size());
}

inline std::uint64_t XXHash3::digest() {
  return XXH3_64bits_digest(&state);
}

inline std::uint32_t xx_hash32(std::span<char const> source, std::uint32_t seed) {
  return XXH32(source.data(), source.size(), seed);
}

inline std::uint32_t xx_hash32(ByteArray const& in) { return xx_hash32(in.span()); }

inline std::uint32_t xx_hash32(String const& in) { return xx_hash32(std::span<char const>(in.utf8())); }

inline std::uint64_t xx_hash64(std::span<char const> source, std::uint64_t seed) {
  return XXH64(source.data(), source.size(), seed);
}

inline std::uint64_t xx_hash64(ByteArray const& in) { return xx_hash64(in.span()); }

inline std::uint64_t xx_hash64(String const& in) { return xx_hash64(std::span<char const>(in.utf8())); }

inline std::uint64_t xx_hash3(std::span<char const> source) { return XXH3_64bits(source.data(), source.size()); }

inline auto xx_hash3_128(std::span<char const> source) { return XXH3_128bits(source.data(), source.size()); }

inline std::uint64_t xx_hash3(ByteArray const& in) { return xx_hash3(in.span()); }

inline std::uint64_t xx_hash3(String const& in) { return xx_hash3(std::span<char const>(in.utf8())); }
}

export module star.xx_hash;

export namespace Star {
using ::Star::hash_append;
using ::Star::hash_append_primitive_value;
using ::Star::hash_append_representation;
using ::Star::hash_appendable;
using ::Star::hash_encode_primitive;
using ::Star::xx_hash3;
using ::Star::xx_hash32;
using ::Star::xx_hash3_128;
using ::Star::xx_hash64;
using ::Star::xx_hash_hasher;
using ::Star::XXHash3;
using ::Star::XXHash32;
using ::Star::XXHash64;
}
