import std;
import star.math_common;
import star.interpolation;
import star.vector;
import star.matrix3;

#include "gtest/gtest.h"

using namespace Star;

TEST(Math, FiniteBoundaryClassificationSurvivesFastMath) {
  for (auto value : {0.0, -0.0, 1.0, -1.0, std::numeric_limits<double>::max(), std::numeric_limits<double>::denorm_min()})
    EXPECT_TRUE(isFinite(value));
  for (auto bits : {0x7ff0000000000000ull, 0xfff0000000000000ull, 0x7ff8000000000001ull, 0xfff8000000000001ull}) {
    volatile std::uint64_t input = bits;
    EXPECT_FALSE(isFinite(std::bit_cast<double>(static_cast<std::uint64_t>(input))));
  }
  for (auto value : {0.0f, -0.0f, 1.0f, -1.0f, std::numeric_limits<float>::max(), std::numeric_limits<float>::denorm_min()})
    EXPECT_TRUE(isFinite(value));
  for (auto bits : {0x7f800000u, 0xff800000u, 0x7fc00001u, 0xffc00001u}) {
    volatile std::uint32_t input = bits;
    EXPECT_FALSE(isFinite(std::bit_cast<float>(static_cast<std::uint32_t>(input))));
  }
}

TEST(Math, CycleIncrement) {
  int a = 0;
  a = cycleIncrement(a, 10, 13);
  ASSERT_EQ(a, 10);
  a = cycleIncrement(a, 10, 13);
  ASSERT_EQ(a, 11);
  a = cycleIncrement(a, 10, 13);
  ASSERT_EQ(a, 12);
  a = cycleIncrement(a, 10, 13);
  ASSERT_EQ(a, 13);
  a = cycleIncrement(a, 10, 13);
  ASSERT_EQ(a, 10);
  int b = 14;
  b = cycleIncrement(b, 10, 13);
  ASSERT_EQ(b, 10);
}

TEST(Math, GenericLerpPreservesArithmetic) {
  EXPECT_EQ(lerp(0.25f, Vec2F(2.0f, -4.0f), Vec2F(10.0f, 4.0f)), Vec2F(4.0f, -2.0f));
  EXPECT_EQ(lerp(2.0f, Vec2F(2.0f, -4.0f), Vec2F(10.0f, 4.0f)), Vec2F(18.0f, 12.0f));
  EXPECT_EQ(lerp(0.25f, -3, 3), -1);
}

TEST(Math, ClampedFloatToBytePreservesSaturationAndTruncation) {
  EXPECT_EQ(floatToByte(-1.0f, true), 0);
  EXPECT_EQ(floatToByte(0.0f, true), 0);
  EXPECT_EQ(floatToByte(0.5f, true), 127);
  EXPECT_EQ(floatToByte(1.0f, true), 255);
  EXPECT_EQ(floatToByte(2.0f, true), 255);
}

TEST(Matrix3, RangeCopyPreservesRowMajorValuesAndDestinationBounds) {
  std::array<int, 9> input{1, 2, 3, 4, 5, 6, 7, 8, 9};
  Matrix3<int> matrix(input);
  input[3] = 99;
  EXPECT_EQ(2, matrix[0][1]);
  EXPECT_EQ(4, matrix[1][0]);

  std::array<int, 11> output;
  output.fill(-1);
  matrix.copy(std::span(output).subspan<1, 9>());
  EXPECT_EQ(output, (std::array<int, 11>{-1, 1, 2, 3, 4, 5, 6, 7, 8, 9, -1}));
}
