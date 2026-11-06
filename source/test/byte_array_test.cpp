import star.byte_array;
import std;
import star.string;
import star.encode;

#include "gtest/gtest.h"

using namespace Star;

TEST(ByteArrayTest, All) {
  auto res = ByteArray::fromCString("foobar");
  res.insert(2, 'a');
  res.insert(6, 'b');
  res.push_back('c');
  res.insert(9, 'd');
  EXPECT_EQ(res, ByteArray::fromCString("foaobabrcd"));

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

  EXPECT_EQ(a->andWith(*b), *e);
  EXPECT_EQ(a->orWith(*b), *d);
  EXPECT_EQ(b->xorWith(*c), *f);
  EXPECT_EQ(g->xorWith(*h), *f);
  EXPECT_EQ(g->xorWith(*h, true), *i);
  EXPECT_EQ(h->xorWith(*g, true), *i);
}

TEST(ByteArrayTest, CompareNormalizesUnsignedBinaryAndPrefixOrdering) {
  std::array<ByteArray, 7> const ordered{ByteArray{},
                                         ByteArray::fromCString("a"),
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
  auto bytes = ByteArray::fromCString("ab");
  bytes.reserve(16);
  bytes.resize(5, static_cast<char>(0x80));
  EXPECT_EQ(bytes, ByteArray(std::span<char const>("ab\x80\x80\x80", 5)));
  bytes.resize(1, static_cast<char>(0xff));
  EXPECT_EQ(bytes, ByteArray::fromCString("a"));
  bytes.resize(3, 'z');
  EXPECT_EQ(bytes, ByteArray::fromCString("azz"));
  bytes.resize(0, 'x');
  EXPECT_TRUE(bytes.empty());
  bytes.resize(2, 'q');
  EXPECT_EQ(bytes, ByteArray::fromCString("qq"));
}
