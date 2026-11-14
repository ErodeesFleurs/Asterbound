import std;
import star.format;
import star.byte_array;
import star.secure_random;

#include "gtest/gtest.h"

using namespace Star;

TEST(SecureRandomTest, EmptySpanAndOwningRequestSucceed) {
  EXPECT_TRUE(secure_random_fill(std::span<char>{}));
  auto bytes = secure_random_bytes(0);
  ASSERT_TRUE(bytes);
  EXPECT_EQ(bytes->size(), 0u);
}

TEST(SecureRandomTest, FillHonorsBorrowedSpanBoundaries) {
  std::array<char, 4098> storage;
  storage.fill('!');
  auto filled = secure_random_fill(std::span(storage).subspan(1, 4096));
  ASSERT_TRUE(filled);
  EXPECT_EQ(storage.front(), '!');
  EXPECT_EQ(storage.back(), '!');
  // No probabilistic assertions about entropy contents: every byte value,
  // including the original sentinel, is a valid OS CSPRNG result.
}
