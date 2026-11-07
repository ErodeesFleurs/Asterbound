import std;
import star.hash;
import star.byte_array;
import star.string;
import star.string_view;
import star.xx_hash;
import star.static_random;

#include "gtest/gtest.h"

namespace HashTestTypes {
struct Value {
  std::size_t value;
};
struct Disabled {};
enum class Enum : unsigned { Value = 17 };
struct StandardValue {
  std::size_t value;
};
struct RvalueOnly {
  std::size_t value;
};
struct LvalueOnly {};
struct Nondefault {};
struct ReferenceValue {
  std::size_t value;
};
struct ThrowingValue {};
struct ThrowingConstruction {};
struct ThrowingDestruction {};
struct ConversionValue {};
struct ExplicitValue {};
struct ImplicitSize {
  operator std::size_t() const noexcept(false) { return 7; }
};
struct ExplicitSize {
  explicit operator std::size_t() const noexcept { return 7; }
};
struct LvalueByte {
  constexpr explicit operator std::size_t() & noexcept { return 23; }
};
struct RvalueByte {
  explicit operator std::size_t() && noexcept { return 23; }
};
struct ThrowingByte {
  explicit operator std::size_t() const noexcept(false) { return 23; }
};
struct TraceValue {
  std::size_t value;
};
inline std::vector<int> trace;
inline int next_hasher = 0;
}// namespace HashTestTypes

namespace std {
template <> struct hash<HashTestTypes::StandardValue> {
  constexpr std::size_t operator()(HashTestTypes::StandardValue const& value) const noexcept {
    return value.value + 19;
  }
};
}// namespace std

namespace Star {
template <> struct hash<HashTestTypes::Value> {
  constexpr std::size_t operator()(HashTestTypes::Value const& value) const noexcept { return value.value; }
};
template <> struct hash<HashTestTypes::RvalueOnly> {
  constexpr hash() noexcept = default;
  hash(hash const&) = delete;
  constexpr std::size_t operator()(HashTestTypes::RvalueOnly const& value) && noexcept { return value.value; }
};
template <> struct hash<HashTestTypes::LvalueOnly> {
  std::size_t operator()(HashTestTypes::LvalueOnly const&) & noexcept { return 1; }
};
template <> struct hash<HashTestTypes::Nondefault> {
  hash() = delete;
  std::size_t operator()(HashTestTypes::Nondefault const&) const noexcept { return 1; }
};
template <> struct hash<HashTestTypes::ReferenceValue&> {
  constexpr std::size_t operator()(HashTestTypes::ReferenceValue& value) const noexcept { return value.value; }
};
template <> struct hash<HashTestTypes::ThrowingValue> {
  std::size_t operator()(HashTestTypes::ThrowingValue const&) const noexcept(false) { return 5; }
};
template <> struct hash<HashTestTypes::ThrowingConstruction> {
  hash() noexcept(false) {}
  std::size_t operator()(HashTestTypes::ThrowingConstruction const&) const noexcept { return 5; }
};
template <> struct hash<HashTestTypes::ThrowingDestruction> {
  ~hash() noexcept(false) {}
  std::size_t operator()(HashTestTypes::ThrowingDestruction const&) const noexcept { return 5; }
};
template <> struct hash<HashTestTypes::ConversionValue> {
  HashTestTypes::ImplicitSize operator()(HashTestTypes::ConversionValue const&) const noexcept { return {}; }
};
template <> struct hash<HashTestTypes::ExplicitValue> {
  HashTestTypes::ExplicitSize operator()(HashTestTypes::ExplicitValue const&) const noexcept { return {}; }
};
template <> struct hash<HashTestTypes::TraceValue> {
  hash() : id(++HashTestTypes::next_hasher) { HashTestTypes::trace.push_back(100 + id); }
  ~hash() { HashTestTypes::trace.push_back(300 + id); }
  std::size_t operator()(HashTestTypes::TraceValue const& value) const {
    HashTestTypes::trace.push_back(200 + static_cast<int>(value.value));
    return value.value;
  }
  int id;
};
}// namespace Star

namespace {
using namespace HashTestTypes;

constexpr std::size_t combined(std::size_t head, std::size_t tail) {
  Star::hash_combine(head, tail);
  return head;
}

template <typename T>
concept pl_puttable = requires(Star::PLHasher& hasher, T value) { hasher.put(value); };

template <typename... T>
concept has_hash_of = requires(T const&... values) { Star::hash_of(values...); };

static_assert(Star::hashable<Value>);
static_assert(Star::hashable<RvalueOnly>);
static_assert(!std::is_copy_constructible_v<Star::hash<RvalueOnly>>);
static_assert(!Star::hashable<LvalueOnly>);
static_assert(Star::hash_invocable<Star::hash<LvalueOnly>&, LvalueOnly const&>);
static_assert(!Star::hash_invocable<Star::hash<LvalueOnly> const&, LvalueOnly const&>);
static_assert(!Star::hashable<Nondefault>);
static_assert(Star::hash_invocable<Star::hash<Nondefault> const&, Nondefault const&>);
static_assert(!Star::hashable<ExplicitValue>);
static_assert(Star::hashable<ConversionValue>);
static_assert(!has_hash_of<>);
static_assert(!has_hash_of<Value, Disabled>);
static_assert(Star::pl_hash_byte<char> && Star::pl_hash_byte<unsigned char>);
static_assert(Star::pl_hash_byte<char32_t> && Star::pl_hash_byte<std::byte>);
static_assert(Star::pl_hash_byte<LvalueByte> && pl_puttable<LvalueByte>);
static_assert(!Star::pl_hash_byte<RvalueByte> && !pl_puttable<RvalueByte>);
static_assert(!Star::pl_hash_byte<void*> && !pl_puttable<void*>);
static_assert(noexcept(Star::PLHasher().put(LvalueByte{})));
static_assert(!noexcept(Star::PLHasher().put(ThrowingByte{})));
static_assert(noexcept(Star::hash_of(Value{1}, Value{2}, Value{3})));
static_assert(!noexcept(Star::hash_of(ThrowingValue{})));
static_assert(!noexcept(Star::hash_of(ThrowingConstruction{})));
static_assert(!noexcept(Star::hash_of(ThrowingDestruction{})));
static_assert(!noexcept(Star::hash<std::tuple<ThrowingConstruction>>{}({})));
static_assert(!noexcept(Star::hash<std::tuple<ThrowingDestruction>>{}({})));
static_assert(!noexcept(Star::hash_of(ConversionValue{})));
static_assert(!noexcept(Star::hash_of(Value{1}, ConversionValue{}, Value{3})));
static_assert(noexcept(Star::hash<std::optional<Value>>{}(std::optional<Value>{})));
static_assert(!noexcept(Star::hash<std::optional<ConversionValue>>{}(std::optional<ConversionValue>{})));
static_assert(!noexcept(Star::hash<std::pair<Value, ThrowingValue>>{}({})));
static_assert(!noexcept(Star::hash<std::tuple<Value, ConversionValue>>{}({})));
static_assert(Star::hash_of(Value{1}, Value{2}, Value{3}) == combined(1, combined(2, 3)));
static_assert(Star::hash<std::pair<Value, Value>>{}({Value{1}, Value{2}}) == combined(1, 2));
static_assert(Star::hash<std::tuple<Value, Value, Value>>{}({Value{1}, Value{2}, Value{3}})
              == combined(1, combined(2, combined(3, 0))));
static_assert(Star::hash<std::tuple<>>{}({}) == 0);
static_assert(Star::hash<std::optional<Value>>{}(std::nullopt) == 0);
static_assert(Star::hash<std::optional<Value>>{}(Value{11}) == 11);
static_assert(Star::hash<std::tuple<RvalueOnly>>{}({RvalueOnly{9}}) == combined(9, 0));
static_assert(!Star::hashable<std::optional<RvalueOnly>>);
static_assert(!Star::hashable<std::pair<RvalueOnly, Value>>);
static_assert(Star::hashable<std::tuple<RvalueOnly>>);
static_assert(!Star::hashable<std::tuple<Nondefault>>);
static_assert(!Star::hashable<std::tuple<ExplicitValue>>);
static_assert(Star::hashable<std::pair<ReferenceValue&, ReferenceValue&>>);
static_assert(Star::hashable<std::tuple<ReferenceValue&>>);
static_assert(!Star::hashable<Disabled>);
static_assert(!Star::hashable<int const>);
static_assert(!Star::hashable<void>);
static_assert(!Star::hashable<std::optional<Disabled>>);
static_assert(!Star::hashable<std::pair<Value, Disabled>>);
static_assert(!Star::hashable<std::tuple<Value, Disabled>>);
static_assert(std::is_base_of_v<std::hash<Disabled>, Star::hash<Disabled>>);
static_assert(!std::is_default_constructible_v<Star::hash<Disabled>>);
static_assert(!std::is_default_constructible_v<Star::hash<std::optional<Disabled>>>);
static_assert(std::is_base_of_v<std::hash<StandardValue>, Star::hash<StandardValue>>);
static_assert(std::is_base_of_v<std::hash<Enum>, Star::hash<Enum>>);
static_assert(std::is_base_of_v<std::hash<Enum>, Star::hash<Enum const>>);
static_assert(!std::is_default_constructible_v<std::hash<Enum const>>);
static_assert(Star::hashable<Enum const>);
static_assert(Star::hash<StandardValue>{}(StandardValue{4}) == 23);
static_assert([] {
  Star::PLHasher hasher(3);
  hasher.put(static_cast<signed char>(-1));
  hasher.put(LvalueByte{});
  return hasher.hash() == (3 * std::size_t{101} + static_cast<std::size_t>(-1)) * 101 + 23;
}());
static_assert(Star::hash<Star::String>{}(std::string_view("a\0b", 3)) == (std::size_t{'a'} * 101) * 101 + 'b');
}// namespace

TEST(HashTest, CompoundHashesKeepDistinctAssociationAndTerminalZero) {
  EXPECT_EQ(Star::hash_of(Value{1}), 1u);
  EXPECT_EQ(Star::hash_of(Value{1}, Value{2}, Value{3}), combined(1, combined(2, 3)));
  auto tuple_hash = Star::hash<std::tuple<Value, Value, Value>>{}({Value{1}, Value{2}, Value{3}});
  EXPECT_EQ(tuple_hash, combined(1, combined(2, combined(3, 0))));
  EXPECT_NE(tuple_hash, Star::hash_of(Value{1}, Value{2}, Value{3}));
  EXPECT_EQ((Star::hash<std::pair<Value, Value>>{}({Value{1}, Value{2}})), combined(1, 2));
  ReferenceValue first{8}, second{13};
  EXPECT_EQ((Star::hash<std::pair<ReferenceValue&, ReferenceValue&>>{}({first, second})), combined(8, 13));
  EXPECT_EQ((Star::hash<std::tuple<ReferenceValue&>>{}({first})), combined(8, 0));
}

TEST(HashTest, HashResultsUseImplicitSizeConversions) {
  EXPECT_EQ(Star::hash_of(ConversionValue{}), 7u);
  EXPECT_EQ(Star::hash<std::optional<ConversionValue>>{}(ConversionValue{}), 7u);
  EXPECT_EQ((Star::hash<std::pair<Value, ConversionValue>>{}({Value{3}, {}})), combined(3, 7));
  EXPECT_EQ((Star::hash<std::tuple<Value, ConversionValue>>{}({Value{3}, {}})), combined(3, combined(7, 0)));
}

TEST(HashTest, OptionalKeepsZeroIdentityAndDoesNotHashAnEmptyPayload) {
  Star::hash<std::optional<TraceValue>> hasher;
  trace.clear();
  EXPECT_EQ(hasher(std::nullopt), 0u);
  EXPECT_TRUE(trace.empty());
  EXPECT_EQ(hasher(TraceValue{7}), 7u);
  EXPECT_EQ(trace, std::vector<int>({207}));
}

TEST(HashTest, TupleAndHashOfDestroyTemporaryHashersBeforeEvaluatingTheTail) {
  std::vector<int> const expected{101, 201, 301, 102, 202, 302, 103, 203, 303};
  trace.clear();
  next_hasher = 0;
  auto tuple_result =
    Star::hash<std::tuple<TraceValue, TraceValue, TraceValue>>{}({TraceValue{1}, TraceValue{2}, TraceValue{3}});
  EXPECT_EQ(trace, expected);
  EXPECT_EQ(tuple_result, combined(1, combined(2, combined(3, 0))));
  trace.clear();
  next_hasher = 0;
  auto variadic_result = Star::hash_of(TraceValue{1}, TraceValue{2}, TraceValue{3});
  EXPECT_EQ(trace, expected);
  EXPECT_EQ(variadic_result, combined(1, combined(2, 3)));
}

TEST(HashTest, PairHashesMembersFromLeftToRight) {
  Star::hash<std::pair<TraceValue, TraceValue>> hasher;
  trace.clear();
  EXPECT_EQ(hasher({TraceValue{1}, TraceValue{2}}), combined(1, 2));
  EXPECT_EQ(trace, std::vector<int>({201, 202}));
}

TEST(HashTest, OwningAndBorrowedByteHashesPreserveSignedCharsAndEmbeddedNulls) {
  char const bytes[]{static_cast<char>(0x80), static_cast<char>(0xff), '\0', 'a', static_cast<char>(0xc3)};
  std::string_view view(bytes, std::size(bytes));
  std::string standard(view);
  Star::String owning(view);
  Star::StringView borrowed(view);
  Star::ByteArray array{std::span<char const>(bytes)};
  std::size_t expected = 0;
  for (char byte : bytes)
    expected = expected * 101 + static_cast<std::size_t>(byte);
  Star::hash<Star::String> hasher;
  EXPECT_EQ(hasher(view), expected);
  EXPECT_EQ(hasher(standard), expected);
  EXPECT_EQ(hasher(owning), expected);
  EXPECT_EQ(hasher(borrowed), expected);
  EXPECT_EQ(Star::hash<Star::ByteArray>{}(array), expected);
  EXPECT_EQ(hasher(std::string_view{}), 0u);
  EXPECT_EQ(hasher(Star::String{}), 0u);
  EXPECT_EQ(Star::hash<Star::ByteArray>{}(Star::ByteArray{}), 0u);
  // Literal adapters retain first-NUL semantics, while explicit views are bounded.
  EXPECT_EQ(hasher("a\0b"), hasher(std::string_view("a")));
  EXPECT_NE(hasher("a\0b"), hasher(std::string_view("a\0b", 3)));
}

TEST(HashTest, StandardHashAndEnumInheritancePreserveResults) {
  EXPECT_EQ(Star::hash<StandardValue>{}(StandardValue{29}), std::hash<StandardValue>{}(StandardValue{29}));
  EXPECT_EQ(Star::hash<Enum>{}(Enum::Value), std::hash<Enum>{}(Enum::Value));
  EXPECT_EQ(Star::hash<Enum const>{}(Enum::Value), std::hash<Enum>{}(Enum::Value));
}

TEST(HashTest, StringHashCoversEveryUtf8Byte) {
  auto hash_of = [](std::string_view str) {
    Star::XXHash64 hash(0);
    Star::hash_append(hash, Star::String(str));
    return hash.digest();
  };

  // These two share their first two bytes but differ afterwards. They used to
  // hash identically because String::size() counts codepoints, not UTF-8 bytes.
  EXPECT_NE(hash_of("a\xC3\xA9"), hash_of("a\xC3\x83"));
}

TEST(HashTest, TextSeedsDoNotCollapseToBoolean) {
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
