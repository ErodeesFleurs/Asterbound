#include "gtest/gtest.h"

import std;
import star.vlq;

using namespace Star;

TEST(VlqTest, All) {
  char buffer[10];
  std::int64_t result;

  EXPECT_EQ(writeVlqI(-1, buffer), 1u);
  EXPECT_EQ(readVlqI(result, buffer), 1u);
  EXPECT_EQ(result, -1);

  EXPECT_EQ(writeVlqI(-65, buffer), 2u);
  EXPECT_EQ(readVlqI(result, buffer), 2u);
  EXPECT_EQ(result, -65);

  EXPECT_EQ(writeVlqI(-64, buffer), 1u);
  EXPECT_EQ(readVlqI(result, buffer), 1u);
  EXPECT_EQ(result, -64);

  EXPECT_EQ(writeVlqI((std::int64_t)1 << 63, buffer), 10u);
  EXPECT_EQ(readVlqI(result, buffer), 10u);
  EXPECT_EQ(result, (std::int64_t)1 << 63);

  EXPECT_EQ(writeVlqI(0, buffer), 1u);
  EXPECT_EQ(readVlqI(result, buffer), 1u);
  EXPECT_EQ(result, 0);
}

TEST(VlqTest, UnsignedGroupBoundariesPreserveWireBytesAndFollowingByte) {
  struct Case {
    std::uint64_t value;
    std::initializer_list<unsigned char> bytes;
  };
  Case const cases[]{
    {0, {0x00}},
    {127, {0x7f}},
    {128, {0x81, 0x00}},
    {16384, {0x81, 0x80, 0x00}},
    {std::uint64_t{1} << 63, {0x81, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00}},
    {std::numeric_limits<std::uint64_t>::max(), {0x81, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f}}};
  for (auto const& test : cases) {
    SCOPED_TRACE(test.value);
    std::array<unsigned char, 11> buffer;
    buffer.fill(0xa5);
    auto written = writeVlqU(test.value, buffer.begin());
    ASSERT_EQ(written, test.bytes.size());
    EXPECT_EQ(vlqUSize(test.value), test.bytes.size());
    EXPECT_TRUE(std::equal(test.bytes.begin(), test.bytes.end(), buffer.begin()));
    EXPECT_EQ(buffer[written], 0xa5);
    std::uint64_t restored = 0;
    EXPECT_EQ(readVlqU(restored, buffer.begin(), written), written);
    EXPECT_EQ(restored, test.value);
  }
}
