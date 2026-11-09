import star.byte_array;
import star.format;
import std;
import star.string;
import star.encode;

#include "gtest/gtest.h"

using namespace Star;

TEST(ByteArrayTest, All) {
  auto res = ByteArray::from_string("foobar");
  res.insert(2, 'a');
  res.insert(6, 'b');
  res.push_back('c');
  res.insert(9, 'd');
  EXPECT_EQ(res, ByteArray::from_string("foaobabrcd"));

  auto a = hexDecode("0a0a0a");
  auto b = hexDecode("a0a0a0");
  auto c = hexDecode("818181");
  auto d = hexDecode("aaaaaa");
  auto e = hexDecode("000000");
  auto f = hexDecode("212121");

  auto g = hexDecode("a0a0a0");
  auto h = hexDecode("8181818181");
  auto i = hexDecode("2121218181");
  ASSERT_TRUE(a);
  ASSERT_TRUE(b);
  ASSERT_TRUE(c);
  ASSERT_TRUE(d);
  ASSERT_TRUE(e);
  ASSERT_TRUE(f);
  ASSERT_TRUE(g);
  ASSERT_TRUE(h);
  ASSERT_TRUE(i);

  EXPECT_EQ(a->bit_and(*b), *e);
  EXPECT_EQ(a->bit_or(*b), *d);
  EXPECT_EQ(b->bit_xor(*c), *f);
  EXPECT_EQ(g->bit_xor(*h), *f);
  EXPECT_EQ(g->bit_xor(*h, true), *i);
  EXPECT_EQ(h->bit_xor(*g, true), *i);
}

TEST(ByteArrayTest, CompareNormalizesUnsignedBinaryAndPrefixOrdering) {
  std::array<ByteArray, 7> const ordered{ByteArray{},
                                         ByteArray::from_string("a"),
                                         ByteArray(std::span<char const>("a\0", 2)),
                                         ByteArray(std::span<char const>("a\0b", 3)),
                                         ByteArray(std::span<char const>("\x7f", 1)),
                                         ByteArray(std::span<char const>("\x80", 1)),
                                         ByteArray(std::span<char const>("\xff", 1))};
  for (std::size_t i = 0; i < ordered.size(); ++i)
    for (std::size_t j = 0; j < ordered.size(); ++j)
      EXPECT_EQ(ordered[i].compare(ordered[j]), (i > j) - (i < j));
}

TEST(ByteArrayTest, ResizeFillPreservesPrefixAndOnlyInitializesGrowth) {
  auto bytes = ByteArray::from_string("ab");
  bytes.reserve(16);
  bytes.resize(5, static_cast<char>(0x80));
  EXPECT_EQ(bytes, ByteArray(std::span<char const>("ab\x80\x80\x80", 5)));
  bytes.resize(1, static_cast<char>(0xff));
  EXPECT_EQ(bytes, ByteArray::from_string("a"));
  bytes.resize(3, 'z');
  EXPECT_EQ(bytes, ByteArray::from_string("azz"));
  bytes.resize(0, 'x');
  EXPECT_TRUE(bytes.empty());
  bytes.resize(2, 'q');
  EXPECT_EQ(bytes, ByteArray::from_string("qq"));
}

namespace {
template <class Owner>
concept borrows_bytes = requires(Owner&& owner) { std::forward<Owner>(owner).span(); };
template <class Owner>
concept borrows_data = requires(Owner&& owner) { std::forward<Owner>(owner).data(); };
template <class Owner>
concept borrows_at = requires(Owner&& owner) { std::forward<Owner>(owner).at(0); };
template <class Owner>
concept borrows_begin = requires(Owner&& owner) { std::forward<Owner>(owner).begin(); };
static_assert(borrows_bytes<ByteArray&> && borrows_bytes<ByteArray const&>);
static_assert(!borrows_bytes<ByteArray> && !borrows_bytes<ByteArray const>);
static_assert(!borrows_data<ByteArray> && !borrows_data<ByteArray const>);
static_assert(!borrows_at<ByteArray> && !borrows_begin<ByteArray>);
static_assert(std::same_as<decltype(std::declval<ByteArray&>().span()), std::span<char>>);
static_assert(std::same_as<decltype(std::declval<ByteArray const&>().span()), std::span<char const>>);
static_assert(byte_combiner<decltype([](char a, char b) { return int(a) + b; })>);
static_assert(byte_combiner<decltype([](char&& a, char&& b) { return int(a) + b; })>);
static_assert(!byte_combiner<decltype([](char&, char&) { return 'a'; })>);
static_assert(!byte_combiner<decltype([](char, char) {})>);
static_assert(!byte_combiner<decltype([](char) { return 'a'; })>);
}

TEST(ByteArrayTest, AppendingSelfAndInteriorSpansPreservesBytesAcrossGrowth) {
  for (std::size_t count : {16u, 32u, 128u, 4096u}) {
    std::string source;
    for (std::size_t i = 0; i != count; ++i) source.push_back(static_cast<char>(i));
    for (bool spare : {false, true}) {
      auto bytes = ByteArray::from_string(source);
      if (spare) bytes.reserve(count * 3);
      bytes.append(bytes);
      EXPECT_EQ(bytes, ByteArray::from_string(source + source));
      bytes = ByteArray::from_string(source);
      if (spare) bytes.reserve(count * 3);
      bytes.append(bytes.span().subspan(3, count - 5));
      EXPECT_EQ(bytes, ByteArray::from_string(source + source.substr(3, count - 5)));
    }
  }
}

TEST(ByteArrayTest, OverlappingWritesAndCopiesWorkInBothDirections) {
  for (bool spare : {false, true}) {
    auto bytes = ByteArray::from_string("abcdef");
    if (spare) bytes.reserve(64);
    bytes.write_from(bytes.span(), 2);
    EXPECT_EQ(bytes, ByteArray::from_string("ababcdef"));
    bytes.write_from(bytes.span().subspan(2, 6), 0);
    EXPECT_EQ(bytes, ByteArray::from_string("abcdefef"));
    bytes = ByteArray::from_string("abcdef");
    bytes.copy_to(bytes.span().subspan(2));
    EXPECT_EQ(bytes, ByteArray::from_string("ababcd"));
    bytes = ByteArray::from_string("abcdef");
    bytes.copy_to(bytes.span().first(4), 2);
    EXPECT_EQ(bytes, ByteArray::from_string("cdefef"));
    bytes.write_from({}, std::numeric_limits<std::size_t>::max());
    EXPECT_EQ(bytes, ByteArray::from_string("cdefef"));
  }
}

TEST(ByteArrayTest, InsertReturnsInsertedElementForEmptyMiddleAndEnd) {
  ByteArray bytes;
  auto first = bytes.insert(bytes.begin(), 'a');
  ASSERT_EQ(first, bytes.begin());
  EXPECT_EQ(*first, 'a');
  auto last = bytes.insert(bytes.end(), 'c');
  ASSERT_EQ(last, bytes.begin() + 1);
  EXPECT_EQ(*last, 'c');
  auto middle = bytes.insert(bytes.begin() + 1, 'b');
  ASSERT_EQ(middle, bytes.begin() + 1);
  EXPECT_EQ(*middle, 'b');
  EXPECT_EQ(bytes, ByteArray::from_string("abc"));
}

TEST(ByteArrayTest, CopyMoveAndClearRetainOwnershipContracts) {
  auto original = ByteArray::from_string(std::string_view("a\0b", 3));
  auto copied = original;
  copied[0] = 'X';
  EXPECT_EQ(original, ByteArray::from_string(std::string_view("a\0b", 3)));
  auto moved = std::move(copied);
  EXPECT_EQ(moved, ByteArray::from_string(std::string_view("X\0b", 3)));
  EXPECT_TRUE(copied.empty());
  copied.push_back('q');
  EXPECT_EQ(copied, ByteArray::from_string("q"));
  auto& same = moved;
  moved = same;
  EXPECT_EQ(moved, ByteArray::from_string(std::string_view("X\0b", 3)));
  moved = std::move(same);
  EXPECT_EQ(moved, ByteArray::from_string(std::string_view("X\0b", 3)));
  auto capacity = moved.capacity();
  moved.clear();
  EXPECT_TRUE(moved.empty());
  EXPECT_EQ(moved.capacity(), capacity);
  moved.reset();
  EXPECT_TRUE(moved.empty());
  EXPECT_EQ(moved.capacity(), 0u);
  moved.append(original);
  EXPECT_EQ(moved, original);
}

TEST(ByteArrayTest, SubstringsClampWithoutOverflowAndOwnTheirBytes) {
  auto bytes = ByteArray::from_string("abcdef");
  auto result = bytes.substr(2, std::numeric_limits<std::size_t>::max());
  bytes[2] = 'X';
  EXPECT_EQ(result, ByteArray::from_string("cdef"));
  EXPECT_TRUE(bytes.substr(6, 1).empty());
  EXPECT_TRUE(bytes.substr(std::numeric_limits<std::size_t>::max(), 1).empty());
  bytes.remove_prefix(2);
  EXPECT_EQ(bytes, ByteArray::from_string("Xdef"));
  bytes.remove_suffix(2);
  EXPECT_EQ(bytes, ByteArray::from_string("Xd"));
  bytes.remove_prefix(std::numeric_limits<std::size_t>::max());
  EXPECT_TRUE(bytes.empty());
}

TEST(ByteArrayTest, CheckedAccessAndNullTerminatedFactoryPreserveBinaryBytes) {
  auto bytes = ByteArray::from_string_with_null(std::string_view("a\0b", 3));
  EXPECT_EQ(bytes, ByteArray(std::span<char const>("a\0b\0", 4)));
  auto element = bytes.at(2);
  ASSERT_TRUE(element);
  EXPECT_EQ(&element->get(), bytes.data() + 2);
  EXPECT_EQ(element->get(), 'b');
  auto invalid = bytes.at(bytes.size());
  ASSERT_FALSE(invalid);
  EXPECT_TRUE(invalid.error().is<OutOfRangeException>());
  EXPECT_FALSE(bytes.at(std::numeric_limits<std::size_t>::max()));
}

TEST(ByteArrayTest, CombineBorrowsMoveOnlyLvalueCallbackAndPreservesOperandOrder) {
  struct callback {
    std::unique_ptr<int> calls = std::make_unique<int>(0);
    char operator()(char&& smaller, char&& larger) & {
      ++*calls;
      auto result = static_cast<char>(smaller - larger);
      smaller = larger = 'X';
      return result;
    }
  } combine;
  const auto small = ByteArray::from_string("ab");
  const auto large = ByteArray::from_string("ABC");
  auto result = large.combine(combine, small, true);
  EXPECT_EQ(*combine.calls, 2);
  EXPECT_EQ(result, ByteArray::from_string("  C"));
  result = small.combine(combine, large, false);
  EXPECT_EQ(*combine.calls, 4);
  EXPECT_EQ(result, ByteArray::from_string("  "));
  EXPECT_EQ(small, ByteArray::from_string("ab"));
  EXPECT_EQ(large, ByteArray::from_string("ABC"));
  EXPECT_EQ(large.bit_xor(ByteArray{}, true), large);
  EXPECT_TRUE(large.bit_xor(ByteArray{}, false).empty());
}

TEST(ByteArrayTest, OversizedGrowthTerminatesBeforeWriting) {
  EXPECT_DEATH({ ByteArray bytes; bytes.reserve(ByteArray::max_size() + 1); }, ".*");
  EXPECT_DEATH({
    auto bytes = ByteArray::from_string("abc");
    bytes.write_from(std::span<char const>("q", 1), std::numeric_limits<std::size_t>::max());
  }, ".*");
}
