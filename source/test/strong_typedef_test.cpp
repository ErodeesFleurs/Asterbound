import std;
import star.strong_typedef;

#include "gtest/gtest.h"

namespace {
using PlayerId = Star::StrongTypedef<int, struct PlayerTag>;
using ItemId = Star::StrongTypedef<int, struct ItemTag>;
using Coordinate = Star::StrongTypedef<float, struct CoordinateTag>;
static_assert(std::is_trivially_copyable_v<PlayerId> && sizeof(PlayerId) == sizeof(int) && alignof(PlayerId) == alignof(int));
static_assert(std::is_copy_assignable_v<PlayerId> && std::is_move_assignable_v<PlayerId>);
static_assert(!std::is_assignable_v<PlayerId&, ItemId> && !std::is_assignable_v<PlayerId&, int>);
static_assert(!std::convertible_to<int, PlayerId> && !std::convertible_to<PlayerId&, int&>);
static_assert(std::same_as<std::compare_three_way_result_t<PlayerId>, std::strong_ordering>);
static_assert(std::same_as<std::compare_three_way_result_t<Coordinate>, std::partial_ordering>);
static_assert(PlayerId{} < PlayerId{1});

TEST(StrongTypedefTest, FloatingOrder) {
  volatile std::uint32_t bits = 0x7fc00001u;
  Coordinate nan{std::bit_cast<float>(std::uint32_t(bits))};
  EXPECT_TRUE((nan <=> Coordinate{}) == std::partial_ordering::unordered);
  EXPECT_FALSE(nan == nan);
  EXPECT_EQ(Coordinate{-0.f}, Coordinate{0.f});
  EXPECT_LT(Coordinate{-2.f}, Coordinate{-1.f});
}
}
