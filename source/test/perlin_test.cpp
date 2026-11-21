import std;
import star.format;
import star.json;
import star.perlin;
import star.string;
import star.string_view;

#include "gtest/gtest.h"

using namespace Star;

namespace {
  void expect_empty(PerlinD const& value) {
    EXPECT_EQ(value.type(), PerlinType::Uninitialized);
    EXPECT_EQ(value.octaves(), 0u);
    EXPECT_EQ(value.frequency(), 0);
    EXPECT_EQ(value.amplitude(), 0);
    EXPECT_EQ(value.bias(), 0);
    EXPECT_EQ(value.alpha(), 0);
    EXPECT_EQ(value.beta(), 0);
    EXPECT_FALSE(value.get(1));
    EXPECT_FALSE(value.get(1, 2));
    EXPECT_FALSE(value.get(1, 2, 3));
    auto actual = value.to_json();
    auto empty = PerlinD{}.to_json();
    ASSERT_EQ(actual.has_value(), empty.has_value());
    if (actual)
      EXPECT_EQ(*actual, *empty);
  }

  void expect_same_samples(PerlinD const& lhs, PerlinD const& rhs) {
    for (auto x : {-1.75, 0.125, 511.75}) {
      auto left = std::array{lhs.get(x), lhs.get(x, -0.25), lhs.get(x, -0.25, 511.5)};
      auto right = std::array{rhs.get(x), rhs.get(x, -0.25), rhs.get(x, -0.25, 511.5)};
      for (std::size_t i = 0; i < left.size(); ++i) {
        ASSERT_TRUE(left[i]);
        ASSERT_TRUE(right[i]);
        EXPECT_EQ(*left[i], *right[i]);
      }
    }
  }
}

TEST(PerlinTest, EmptyCopiesAndMovedFromObjectsHaveCanonicalState) {
  static_assert(std::is_nothrow_move_constructible_v<PerlinD>);
  static_assert(std::is_nothrow_move_assignable_v<PerlinD>);
  PerlinD empty;
  expect_empty(empty);
  PerlinD copied_empty(empty);
  expect_empty(copied_empty);
  auto original = PerlinD::create(JsonObject{{"type", "billow"}, {"octaves", 3}}, 913);
  ASSERT_TRUE(original);
  PerlinD copied(*original);
  PerlinD moved(std::move(*original));
  expect_empty(*original);
  expect_same_samples(copied, moved);
  copied = empty;
  expect_empty(copied);
  copied = moved;
  moved = empty;
  expect_empty(moved);
  moved = std::move(copied);
  expect_empty(copied);
  *original = moved;
  expect_same_samples(moved, *original);
}

TEST(PerlinTest, ExplicitSeedOverridesWithoutMutatingConfig) {
  Json config = JsonObject{{"type", "ridgedMulti"}, {"seed", "ignored"}, {"octaves", 3},
      {"bias", -0.25}, {"gain", 1.75}, {"offset", 0.75}};
  constexpr std::uint64_t seed = 0x8030000000000321ull;
  auto override = PerlinD::create(config, seed);
  ASSERT_TRUE(override);
  EXPECT_FALSE(PerlinD::create(config));
  auto encoded = override->to_json();
  ASSERT_TRUE(encoded);
  auto actual_seed = encoded->get_uint("seed");
  ASSERT_TRUE(actual_seed);
  EXPECT_EQ(*actual_seed, seed);
  auto restored = PerlinD::create(*encoded);
  ASSERT_TRUE(restored);
  expect_same_samples(*override, *restored);
  auto other = PerlinD::create(config, seed + 1);
  ASSERT_TRUE(other);
  auto first = override->get(0.125, -0.25, 1.75);
  auto second = other->get(0.125, -0.25, 1.75);
  ASSERT_TRUE(first);
  ASSERT_TRUE(second);
  EXPECT_NE(*first, *second);
}

TEST(PerlinTest, WideLatticeCoordinatesMatchWrappedAxes) {
  auto noise = PerlinD::create(JsonObject{{"type", "perlin"}}, 637);
  ASSERT_TRUE(noise);
  for (double x : {2147483647.75, -2147483648.25, 0x1p42 + 135.25,
                   -0x1p42 - 17.75, std::numeric_limits<double>::max()}) {
    double wrapped = std::fmod(x, 512.0);
    auto wide = std::array{noise->get(x), noise->get(-0.25, x), noise->get(-0.25, 0.625, x)};
    auto narrow = std::array{noise->get(wrapped), noise->get(-0.25, wrapped), noise->get(-0.25, 0.625, wrapped)};
    for (std::size_t i = 0; i < wide.size(); ++i) {
      ASSERT_TRUE(wide[i]);
      ASSERT_TRUE(narrow[i]);
      EXPECT_EQ(*wide[i], *narrow[i]);
    }
  }
}

TEST(PerlinTest, RejectsNonfiniteSampledAxesAtEveryOctave) {
  for (String type : {String("perlin"), String("billow"), String("ridgedMulti")}) {
    auto noise = PerlinF::create(JsonObject{{"type", type}, {"frequency", 2}, {"beta", 4}, {"octaves", 2}}, 33);
    ASSERT_TRUE(noise);
    float limit = std::numeric_limits<float>::max();
    EXPECT_FALSE(noise->get(limit));
    EXPECT_FALSE(noise->get(0.25f, limit / 4));
    for (auto bits : {0x7f800000u, 0xff800000u, 0x7fc00001u}) {
      volatile std::uint32_t input = bits;
      auto invalid = std::bit_cast<float>(static_cast<std::uint32_t>(input));
      EXPECT_FALSE(noise->get(0.25f, 1.25f, invalid));
    }
  }
}

TEST(PerlinTest, ParsedParametersAreReusableAndNormalizeOctaves) {
  Json config = JsonObject{{"type", "perlin"}, {"octaves", 2.875}, {"frequency", -0.5},
      {"amplitude", -1}, {"alpha", -2}, {"beta", -3}};
  auto parameters = PerlinD::parse_parameters(config);
  ASSERT_TRUE(parameters);
  for (std::uint64_t seed : {1u, 913u}) {
    PerlinD reused(*parameters, seed);
    auto direct = PerlinD::create(PerlinType::Perlin, 2, -0.5, -1, 0, -2, -3, seed);
    ASSERT_TRUE(direct);
    EXPECT_EQ(reused.octaves(), 2u);
    expect_same_samples(reused, *direct);
  }
  auto sentinel = PerlinD::create(JsonObject{{"type", "uninitialized"}, {"frequency", 12}}, 99);
  ASSERT_TRUE(sentinel);
  expect_empty(*sentinel);
  auto encoded = sentinel->to_json();
  ASSERT_TRUE(encoded);
  auto restored = PerlinD::create(*encoded);
  ASSERT_TRUE(restored);
  expect_empty(*restored);
}

TEST(PerlinTest, RejectsInvalidConfigurationBeforeNarrowing) {
  for (double octaves : {-0.25, -1.0, 0x1p31}) {
    EXPECT_FALSE(PerlinD::create(PerlinType::Perlin, octaves, 1, 1, 0, 2, 2, 1));
    EXPECT_FALSE(PerlinD::parse_parameters(JsonObject{{"type", "perlin"}, {"octaves", octaves}}));
  }
  for (String key : {String("frequency"), String("amplitude"), String("bias"), String("alpha"),
                     String("beta"), String("gain"), String("offset")}) {
    EXPECT_FALSE(PerlinF::parse_parameters(JsonObject{{"type", "perlin"}, {key, 0x1p128}}));
  }
  EXPECT_FALSE(PerlinF::create(PerlinType::Perlin, 2, 1, 1, 0, 0, 2, 1));
  EXPECT_FALSE(PerlinF::create(PerlinType::Perlin, 2, 1, 1, 0, 0x1p-160, 2, 1));
  EXPECT_TRUE(PerlinF::create(PerlinType::Perlin, 1, 1, 1, 0, 0, 2, 1));
  EXPECT_FALSE(PerlinD::create(JsonObject{{"type", "unknown"}}, 1));
  EXPECT_FALSE(PerlinD::create(JsonObject{{"type", "billow"}}));
  for (auto bits : {0x7ff0000000000000ull, 0xfff0000000000000ull, 0x7ff8000000000001ull}) {
    volatile std::uint64_t input = bits;
    auto invalid = std::bit_cast<double>(static_cast<std::uint64_t>(input));
    EXPECT_FALSE(PerlinD::create(PerlinType::Billow, invalid, 1, 1, 0, 2, 2, 1));
    EXPECT_FALSE(PerlinD::create(PerlinType::RidgedMulti, 2, invalid, 1, 0, 2, 2, 1));
  }
}

TEST(PerlinTest, ZeroOctavesKeepModeSpecificOutput) {
  for (auto [type, expected] : {std::pair{PerlinType::Perlin, 3.0},
           std::pair{PerlinType::Billow, 4.0}, std::pair{PerlinType::RidgedMulti, 1.0}}) {
    auto noise = PerlinD::create(type, 0, 1, 2, 3, 0, 0, 19);
    ASSERT_TRUE(noise);
    for (auto sample : {noise->get(0.25), noise->get(-0.25, 1.0), noise->get(0.75, -1.5, 0.25)}) {
      ASSERT_TRUE(sample);
      EXPECT_EQ(*sample, expected);
    }
  }
}
