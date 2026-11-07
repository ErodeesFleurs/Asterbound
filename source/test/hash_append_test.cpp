import std;
import star.string;
import star.xx_hash;
import star.static_random;

#include "gtest/gtest.h"

namespace HashAppendCustom {

struct IntConvertible {
  operator int() const { return -7; }
};

struct Unsupported {};
enum PromotedEnum { PromotedValue = -7 };
enum class ScopedEnum { Value = -7 };

struct VoidValue {
  int value;
};

template <Star::xx_hash_hasher Hasher> void hash_append(Hasher& hash, VoidValue const& value) {
  Star::hash_append(hash, value.value);
}

struct DeferredValue {
  int value;
  std::vector<int>* events;
};

template <typename Hasher> struct AppendCompletion {
  Hasher& hash;
  DeferredValue value;

  AppendCompletion(Hasher& hash, DeferredValue value) : hash(hash), value(value) {}
  AppendCompletion(AppendCompletion const&) = delete;
  AppendCompletion(AppendCompletion&&) = delete;

  ~AppendCompletion() {
    value.events->push_back(-value.value);
    Star::hash_append(hash, value.value + 100);
  }
};

template <Star::xx_hash_hasher Hasher> AppendCompletion<Hasher> hash_append(Hasher& hash, DeferredValue const& value) {
  value.events->push_back(value.value);
  Star::hash_append(hash, value.value);
  return {hash, value};
}

struct Only32 {};
inline void hash_append(Star::XXHash32& hash, Only32 const&) { Star::hash_append(hash, 7); }

// Selection only needs advance + dereference, not a default-constructible,
// equality-comparable or copyable standard input_iterator.
struct SelectionOnlyContainer {
  using value_type = int;
  int values[3]{11, 22, 33};

  struct Iterator {
    using value_type = int;
    using difference_type = std::ptrdiff_t;
    using pointer = int*;
    using reference = int&;
    using iterator_category = std::input_iterator_tag;
    int* current;

    explicit Iterator(int* current) : current(current) {}
    Iterator(Iterator const&) = delete;
    Iterator(Iterator&&) = delete;
    void operator++() { ++current; }
    int& operator*() const { return *current; }
  };

  struct EmptyResult {
    explicit operator bool() const { return false; }
  };

  EmptyResult empty() const { return {}; }
  int size() const { return 3; }
  Iterator begin() { return Iterator(values); }
};

}// namespace HashAppendCustom

namespace {

template <typename Value>
concept static_hash32_input = requires(Value const& value) { Star::static_random_hash32(value); };

template <typename Value>
concept static_hash64_input = requires(Value const& value) { Star::static_random_hash64(value); };

template <typename Container>
concept random_reference_container = requires(Container& container) { Star::static_random_from(container, 7); };

template <typename Container>
concept random_value_container = requires(Container const& container) { Star::static_random_value_from(container, 7); };

template <typename Container>
concept random_shuffle_container = requires(Container& container) { Star::static_random_shuffle(container, 7); };

template <typename Hasher> constexpr bool append_domain() {
  return Star::hash_appendable<Hasher, bool> && Star::hash_appendable<Hasher, int>
    && Star::hash_appendable<Hasher, unsigned int> && Star::hash_appendable<Hasher, long>
    && Star::hash_appendable<Hasher, unsigned long> && Star::hash_appendable<Hasher, long long>
    && Star::hash_appendable<Hasher, unsigned long long> && Star::hash_appendable<Hasher, float>
    && Star::hash_appendable<Hasher, double> && Star::hash_appendable<Hasher, char>
    && Star::hash_appendable<Hasher, signed char> && Star::hash_appendable<Hasher, unsigned char>
    && Star::hash_appendable<Hasher, short> && Star::hash_appendable<Hasher, unsigned short>
    && Star::hash_appendable<Hasher, HashAppendCustom::PromotedEnum>
    && Star::hash_appendable<Hasher, HashAppendCustom::IntConvertible> && Star::hash_appendable<Hasher, std::string>
    && Star::hash_appendable<Hasher, std::string_view> && Star::hash_appendable<Hasher, Star::String>
    && Star::hash_appendable<Hasher, char[4]> && Star::hash_appendable<Hasher, HashAppendCustom::VoidValue>
    && Star::hash_appendable<Hasher, HashAppendCustom::DeferredValue> && !Star::hash_appendable<Hasher const, int>
    && !Star::hash_appendable<Hasher, HashAppendCustom::Unsupported>
    && !Star::hash_appendable<Hasher, HashAppendCustom::ScopedEnum> && !Star::hash_appendable<Hasher, int*>
    && !Star::hash_appendable<Hasher, std::nullptr_t> && !Star::hash_appendable<Hasher, long double>;
}

static_assert(append_domain<Star::XXHash32>());
static_assert(append_domain<Star::XXHash64>());
static_assert(append_domain<Star::XXHash3>());
static_assert(!Star::hash_appendable<int, int>);
static_assert(!Star::hash_appendable<Star::XXHash32, char const*>);
static_assert(!Star::hash_appendable<Star::XXHash64, char const*>);
static_assert(Star::hash_appendable<Star::XXHash3, char const*>);
static_assert(Star::hash_appendable<Star::XXHash3, char*>);
static_assert(static_hash32_input<HashAppendCustom::Only32>);
static_assert(!static_hash64_input<HashAppendCustom::Only32>);
static_assert(!static_hash32_input<HashAppendCustom::Unsupported>);
static_assert(!static_hash64_input<HashAppendCustom::Unsupported>);
static_assert(random_reference_container<std::vector<int>>);
static_assert(random_reference_container<std::vector<int> const>);
static_assert(random_reference_container<std::list<int>>);
static_assert(!random_reference_container<std::vector<bool>>);
static_assert(!random_reference_container<int>);
static_assert(random_reference_container<HashAppendCustom::SelectionOnlyContainer>);
static_assert(!std::input_iterator<HashAppendCustom::SelectionOnlyContainer::Iterator>);
static_assert(random_value_container<std::vector<int>>);
static_assert(random_value_container<std::vector<bool>>);
static_assert(!random_value_container<std::vector<std::unique_ptr<int>>>);
static_assert(random_shuffle_container<std::vector<int>>);
static_assert(!random_shuffle_container<std::vector<int> const>);
static_assert(!random_shuffle_container<std::list<int>>);
static_assert(
  std::same_as<decltype(Star::hash_append(std::declval<Star::XXHash3&>(), std::declval<Star::String const&>())), void>);

// Each expected byte string is independent of native endianness and long's
// platform width. This also protects promotions, signed extension and IEEE bits.
template <typename Hasher, typename Value> void expect_bytes(Value const& value, std::string_view bytes) {
  Hasher appended;
  using Star::hash_append;
  hash_append(appended, value);
  Hasher raw;
  raw.push(std::span<char const>(bytes));
  EXPECT_EQ(appended.digest(), raw.digest());
}

template <typename Hasher> void check_primitive_bytes() {
  expect_bytes<Hasher>(false, std::string_view("\0", 1));
  expect_bytes<Hasher>(true, std::string_view("\1", 1));
  expect_bytes<Hasher>(-2, std::string_view("\xFE\xFF\xFF\xFF", 4));
  expect_bytes<Hasher>(0x12345678u, std::string_view("\x78\x56\x34\x12", 4));
  expect_bytes<Hasher>(-2l, std::string_view("\xFE\xFF\xFF\xFF\xFF\xFF\xFF\xFF", 8));
  expect_bytes<Hasher>(-2ll, std::string_view("\xFE\xFF\xFF\xFF\xFF\xFF\xFF\xFF", 8));
  expect_bytes<Hasher>(0x12345678ul, std::string_view("\x78\x56\x34\x12\0\0\0\0", 8));
  expect_bytes<Hasher>(0x0123456789ABCDEFull, std::string_view("\xEF\xCD\xAB\x89\x67\x45\x23\x01", 8));
  expect_bytes<Hasher>(1.5f, std::string_view("\0\0\xC0\x3F", 4));
  expect_bytes<Hasher>(1.5, std::string_view("\0\0\0\0\0\0\xF8\x3F", 8));
  expect_bytes<Hasher>(std::bit_cast<float>(0x80000000u), std::string_view("\0\0\0\x80", 4));
  expect_bytes<Hasher>(std::bit_cast<double>(0x8000000000000000ull), std::string_view("\0\0\0\0\0\0\0\x80", 8));
  expect_bytes<Hasher>(std::bit_cast<float>(0x7FC12345u), std::string_view("\x45\x23\xC1\x7F", 4));
  expect_bytes<Hasher>(static_cast<signed char>(-7), std::string_view("\xF9\xFF\xFF\xFF", 4));
  expect_bytes<Hasher>(static_cast<unsigned char>(0xA5), std::string_view("\xA5\0\0\0", 4));
  expect_bytes<Hasher>(static_cast<short>(-7), std::string_view("\xF9\xFF\xFF\xFF", 4));
  expect_bytes<Hasher>(HashAppendCustom::PromotedValue, std::string_view("\xF9\xFF\xFF\xFF", 4));
  expect_bytes<Hasher>(HashAppendCustom::IntConvertible{}, std::string_view("\xF9\xFF\xFF\xFF", 4));
  expect_bytes<Hasher>(HashAppendCustom::VoidValue{-7}, std::string_view("\xF9\xFF\xFF\xFF", 4));
}

template <typename Hasher> void check_text_bytes() {
  std::string bytes("a\0\xC3\xA9\xF0\x9F\x8C\x8D", 8);
  expect_bytes<Hasher>(std::string_view(bytes), bytes);
  expect_bytes<Hasher>(bytes, bytes);
  expect_bytes<Hasher>(Star::String(bytes), bytes);
  expect_bytes<Hasher>("a\0\xC3\xA9", "a");
  char bounded[] = {'a', 'b', 'c'};
  expect_bytes<Hasher>(bounded, "abc");
  char terminated[] = {'a', '\0', 'b'};
  expect_bytes<Hasher>(terminated, "a");
  expect_bytes<Hasher>("", std::string_view{});
  expect_bytes<Hasher>(Star::String(std::string("\xFF\0\x80", 3)), std::string_view("\xFF\0\x80", 3));

  // Distinct UTF-8 suffixes used to collapse in the XXH3 String overload.
  Hasher first;
  Hasher second;
  Star::hash_append(first, Star::String("a\xC3\xA9"));
  Star::hash_append(second, Star::String("a\xC3\x83"));
  EXPECT_NE(first.digest(), second.digest());
}

template <typename Hasher> void check_fragmentation() {
  std::string bytes(600, '\0');
  for (std::size_t i = 0; i != bytes.size(); ++i)
    bytes[i] = static_cast<char>((i * 37 + 11) & 0xFF);
  Hasher raw;
  raw.push(std::span<char const>(bytes));
  for (std::size_t chunk : {1u, 3u, 7u, 16u, 31u, 64u, 129u}) {
    Hasher fragmented;
    for (std::size_t offset = 0; offset < bytes.size(); offset += chunk)
      Star::hash_append(fragmented, std::string_view(bytes).substr(offset, std::min(chunk, bytes.size() - offset)));
    EXPECT_EQ(fragmented.digest(), raw.digest());
  }
}

template <typename T, typename... Rest> void expect_static_stream_parity(T const& value, Rest const&... rest) {
  Star::XXHash32 stream32(2938728349u);
  Star::XXHash64 stream64(1997293021376312589);
  using Star::hash_append;
  hash_append(stream32, value);
  ([&] { hash_append(stream32, rest); }(), ...);
  hash_append(stream64, value);
  ([&] { hash_append(stream64, rest); }(), ...);
  EXPECT_EQ(Star::static_random_hash32(value, rest...), stream32.digest());
  EXPECT_EQ(Star::static_random_hash64(value, rest...), stream64.digest());
}

}// namespace

TEST(HashAppendTest, PrimitiveRepresentationMatchesHistoricalBytes) {
  check_primitive_bytes<Star::XXHash32>();
  check_primitive_bytes<Star::XXHash64>();
  check_primitive_bytes<Star::XXHash3>();
}

TEST(HashAppendTest, TextAppendsFullBoundsWithoutUtf8Validation) {
  check_text_bytes<Star::XXHash32>();
  check_text_bytes<Star::XXHash64>();
  check_text_bytes<Star::XXHash3>();
}

TEST(HashAppendTest, StreamingFragmentationPreservesDigest) {
  check_fragmentation<Star::XXHash32>();
  check_fragmentation<Star::XXHash64>();
  check_fragmentation<Star::XXHash3>();
}

TEST(HashAppendTest, Xxh3RetainsCStringPointerArguments) {
  char const* immutable = "a\0suffix";
  char mutableText[] = {'a', '\0', 'b'};
  char* mutablePointer = mutableText;
  expect_bytes<Star::XXHash3>(immutable, "a");
  expect_bytes<Star::XXHash3>(mutablePointer, "a");
}

TEST(HashAppendTest, OneShotMatchesStreamedBytes) {
  Star::String text(std::string("a\0\xC3\xA9", 4));
  Star::XXHash32 hash32;
  Star::XXHash64 hash64;
  Star::XXHash3 hash3;
  Star::hash_append(hash32, text);
  Star::hash_append(hash64, text);
  Star::hash_append(hash3, text);
  EXPECT_EQ(hash32.digest(), Star::xx_hash32(text));
  EXPECT_EQ(hash64.digest(), Star::xx_hash64(text));
  EXPECT_EQ(hash3.digest(), Star::xx_hash3(text));
}

TEST(HashAppendTest, TextAndBooleanSeedsKeepGoldens) {
  EXPECT_EQ(620815165u, Star::static_random_hash32("SystemWorldGeneration"));
  EXPECT_EQ(15694224458012596113ull, Star::static_random_hash64("SystemWorldGeneration"));
  std::string bounded("a\0\xC3\xA9", 4);
  EXPECT_EQ(3636065869u, Star::static_random_hash32(bounded));
  EXPECT_EQ(15767761046075041944ull, Star::static_random_hash64(bounded));
  EXPECT_EQ(708170612u, Star::static_random_hash32("a\0\xC3\xA9"));
  EXPECT_EQ(1230765182578524663ull, Star::static_random_hash64("a\0\xC3\xA9"));
  EXPECT_EQ(2612236624u, Star::static_random_hash32(true));
  EXPECT_EQ(2438509545873450692ull, Star::static_random_hash64(true));
}

TEST(HashAppendTest, AdlReturnTemporariesCompleteBeforeNextAppend) {
  std::vector<int> events;
  auto first = HashAppendCustom::DeferredValue{1, &events};
  auto second = HashAppendCustom::DeferredValue{2, &events};
  auto third = HashAppendCustom::DeferredValue{3, &events};
  EXPECT_EQ(Star::static_random_hash32(first, second, third), Star::static_random_hash32(1, 101, 2, 102, 3, 103));
  EXPECT_EQ(events, (std::vector<int>{1, -1, 2, -2, 3, -3}));
  events.clear();
  EXPECT_EQ(Star::static_random_hash64(first, second, third), Star::static_random_hash64(1, 101, 2, 102, 3, 103));
  EXPECT_EQ(events, (std::vector<int>{1, -1, 2, -2, 3, -3}));
  EXPECT_EQ(Star::static_random_hash32(HashAppendCustom::VoidValue{7}), Star::static_random_hash32(7));
  EXPECT_EQ(Star::static_random_hash64(HashAppendCustom::VoidValue{7}), Star::static_random_hash64(7));
}

TEST(HashAppendTest, ContainerSelectionPreservesBorrowAndEmptyValuePolicy) {
  std::vector<int> values{11, 22, 33};
  auto index = Star::static_random_i32_range(0, 2, "selection");
  auto selected = Star::static_random_from(values, "selection");
  ASSERT_TRUE(selected);
  EXPECT_EQ(&selected->get(), &values[index]);
  auto const& constValues = values;
  auto constSelected = Star::static_random_from(constValues, "selection");
  ASSERT_TRUE(constSelected);
  EXPECT_EQ(&constSelected->get(), &values[index]);
  EXPECT_EQ(Star::static_random_value_from(values, "selection"), values[index]);
  HashAppendCustom::SelectionOnlyContainer selectionOnly;
  auto minimallySelected = Star::static_random_from(selectionOnly, "selection");
  ASSERT_TRUE(minimallySelected);
  EXPECT_EQ(&minimallySelected->get(), &selectionOnly.values[index]);
  std::vector<int> empty;
  EXPECT_FALSE(Star::static_random_from(empty, 7));
  EXPECT_EQ(Star::static_random_value_from(empty, 7), 0);
}

TEST(HashAppendTest, SeededOneShotMatchesNativeStreaming) {
  std::string bytes("a\0\xC3\xA9", 4);
  Star::XXHash32 stream32(0x12345678u);
  Star::XXHash64 stream64(0x123456789ABCDEFull);
  stream32.push(std::span<char const>(bytes));
  stream64.push(std::span<char const>(bytes));
  EXPECT_EQ(Star::xx_hash32(std::span<char const>(bytes), 0x12345678u), stream32.digest());
  EXPECT_EQ(Star::xx_hash64(std::span<char const>(bytes), 0x123456789ABCDEFull), stream64.digest());
}

TEST(HashAppendTest, BoundedPackingPreservesPrimitiveLimitsAndBits) {
  expect_static_stream_parity(true, std::numeric_limits<int>::min(), std::numeric_limits<unsigned int>::max(),
                              std::numeric_limits<long>::min(), std::numeric_limits<unsigned long>::max(),
                              std::numeric_limits<long long>::min(), std::numeric_limits<unsigned long long>::max(),
                              std::bit_cast<float>(0x80000000u), std::bit_cast<double>(0x7FF8123456789ABCull));
  expect_static_stream_parity(-19, 23u, -29l, 31ul, -37ll, 41ull, std::bit_cast<float>(0x7FC12345u),
                              std::bit_cast<double>(0x8000000000000000ull), false);
}

TEST(HashAppendTest, BoundedPackingHandlesExactAndOver64ByteInputs) {
  // Eight canonical 64-bit representations fit exactly; the final bool forces
  // streaming. This remains the same boundary on Windows with 32-bit long.
  expect_static_stream_parity(1ll, -2ll, 3ll, -4ll, 5ll, -6ll, 7ll, -8ll);
  expect_static_stream_parity(1ll, -2ll, 3ll, -4ll, 5ll, -6ll, 7ll, -8ll, true);
  char exact[64];
  char over[65];
  std::fill(std::begin(exact), std::end(exact), 'x');
  std::fill(std::begin(over), std::end(over), 'x');
  expect_static_stream_parity(exact);
  expect_static_stream_parity(over);
  char withBool[63];
  std::fill(std::begin(withBool), std::end(withBool), 'y');
  expect_static_stream_parity(withBool, true);
  // The upper bound, not a potentially overflowing sum or unbounded string
  // scan, controls eligibility even when a large array starts with NUL.
  char large[4096]{};
  expect_static_stream_parity(large, large, large, large);
}

TEST(HashAppendTest, PackedTextKeepsFirstNulAndRawArrayBounds) {
  char raw[] = {'a', '\xC3', '\xA9'};
  char embedded[] = {'a', '\0', 'b', '\xFF'};
  expect_static_stream_parity(-7, raw, 11ull, embedded, false);
  expect_static_stream_parity("a\0hidden", 19l, "\xC3\xA9", 23);
  EXPECT_EQ(Star::static_random_hash32(embedded), Star::static_random_hash32("a"));
  EXPECT_EQ(Star::static_random_hash64(embedded), Star::static_random_hash64("a"));
}

TEST(HashAppendTest, NonExactInputsKeepStreamingOverloadAndAdlPolicy) {
  std::string bounded("a\0\xC3\xA9", 4);
  expect_static_stream_parity(bounded, 7);
  expect_static_stream_parity(std::string_view(bounded), 7);
  expect_static_stream_parity(Star::String(bounded), 7);
  expect_static_stream_parity(static_cast<short>(-7), static_cast<unsigned char>(0xA5), HashAppendCustom::PromotedValue,
                              HashAppendCustom::IntConvertible{});
  expect_static_stream_parity(11, HashAppendCustom::VoidValue{7}, "label", 17ull);
}
