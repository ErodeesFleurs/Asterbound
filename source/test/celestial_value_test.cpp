import std;
import star.celestial_coordinate;
import star.celestial_parameters;
import star.data_stream_devices;
import star.format;
import star.json;
import star.string;
import star.vector;
#include "gtest/gtest.h"

using namespace Star;

namespace {

TEST(CelestialCoordinate, RoundTripsEveryHierarchyInTextFilenameAndJson) {
  Vec3I const location{-17, 5, -9};
  CelestialCoordinate const system(location), planet(location, 2), moon(location, 2, 7);
  for (auto const& [coordinate, id] : {std::pair{CelestialCoordinate(), String("null")},
           std::pair{system, String("-17:5:-9")}, std::pair{planet, String("-17:5:-9:2")},
           std::pair{moon, String("-17:5:-9:2:7")}}) {
    EXPECT_EQ(coordinate.id(), id);
    for (Json const& representation : {Json(id), Json(coordinate.filename()), coordinate.to_json()}) {
      auto decoded = CelestialCoordinate::create(representation);
      ASSERT_TRUE(decoded) << decoded.error().what();
      EXPECT_EQ(*decoded, coordinate);
    }
  }
  for (std::string_view text : {"", "nUlL", "0:0:0"})
    EXPECT_EQ(CelestialCoordinate::create(Json(text)), CelestialCoordinate());
  EXPECT_EQ(CelestialCoordinate::create("  -17_5::-9 2_7 "), moon);
  EXPECT_EQ(CelestialCoordinate::create("-17:5:-9:2:0"), planet);
  EXPECT_TRUE(system.is_system());
  EXPECT_TRUE(planet.is_planetary_body());
  EXPECT_TRUE(moon.is_satellite_body());
  EXPECT_EQ(system.child(2), planet);
  EXPECT_EQ(planet.child(7), moon);
  EXPECT_EQ(moon.parent(), planet);
  EXPECT_EQ(planet.parent(), system);
  EXPECT_EQ(moon.planet(), planet);
  EXPECT_EQ(moon.system(), system);
  EXPECT_EQ(system.orbit_number(), 0);
  EXPECT_EQ(moon.orbit_number(), 7);
  EXPECT_FALSE(system.parent());
  EXPECT_FALSE(system.child(0));
  EXPECT_FALSE(planet.child(-1));
  EXPECT_FALSE(moon.child(1));
  EXPECT_FALSE(CelestialCoordinate().child(1));
}

TEST(CelestialCoordinate, RejectsMalformedHierarchyAndNarrowing) {
  for (std::string_view text : {"1:2", "1:2:3:0:1", "1:2:3:-1", "1:2:3:1:-1", "1:2:3:1:2:3",
           "1:2:3:2147483648", "-2147483649:2:3", "5x:2:3"})
    EXPECT_FALSE(CelestialCoordinate::create(Json(text))) << text;
  for (Json const& location : {Json(JsonArray{1, 2}), Json(JsonArray{1, 2, 3, 4}),
           Json(JsonArray{1, 2147483648LL, 3}), Json(JsonArray{-2147483649LL, 2, 3}), Json(JsonArray{1, 2, 0.5})})
    EXPECT_FALSE(CelestialCoordinate::create(JsonObject{{"location", location}}));
  for (auto const& [planet, satellite] : {std::pair{0LL, 1LL}, std::pair{-1LL, 0LL},
           std::pair{1LL, -1LL}, std::pair{4294967297LL, 1LL}, std::pair{2LL, 2147483648LL}})
    EXPECT_FALSE(CelestialCoordinate::create(JsonObject{{"location", JsonArray{1, 2, 3}},
        {"planet", planet}, {"satellite", satellite}}));
  EXPECT_EQ(CelestialCoordinate::create(JsonObject{{"location", JsonArray{1.0, -2, 3}}, {"planet", Json()}}),
      CelestialCoordinate(Vec3I{1, -2, 3}));
  EXPECT_FALSE(celestial_integer<unsigned>(-1));
  EXPECT_FALSE(celestial_integer<std::size_t>(0x1p64));
  EXPECT_EQ(celestial_integer<unsigned>(4294967295LL), 4294967295U);
}

TEST(CelestialCoordinate, DistanceWidensBeforePlanarSubtraction) {
  auto lo = std::numeric_limits<int>::min(), hi = std::numeric_limits<int>::max();
  CelestialCoordinate const first({lo, hi, 4}, 3), second({hi, lo, 6}, 2, 1);
  double span = double(hi) - lo;
  EXPECT_DOUBLE_EQ(first.distance(second), std::hypot(span, span));
  EXPECT_DOUBLE_EQ(second.distance(first), std::hypot(span, span));
  EXPECT_EQ(first.distance(CelestialCoordinate({lo, hi, lo})), 0.0);
}

TEST(CelestialCoordinate, LegacyWireBytesAndTransactionalHierarchyCheck) {
  CelestialCoordinate const moon({-1, 2, 3}, 4, 5);
  DataStreamBuffer stream;
  ASSERT_TRUE(stream.write(moon));
  constexpr std::array<unsigned char, 20> wire{255, 255, 255, 255, 0, 0, 0, 2, 0, 0, 0, 3, 0, 0, 0, 4, 0, 0, 0, 5};
  ASSERT_EQ(stream.size(), wire.size());
  for (std::size_t i = 0; i < wire.size(); ++i)
    EXPECT_EQ(static_cast<unsigned char>(stream.span()[i]), wire[i]);
  ASSERT_TRUE(stream.seek(0));
  CelestialCoordinate result;
  ASSERT_TRUE(stream.read(result));
  EXPECT_EQ(result, moon);
  // Satellite five without a planet must not replace the previous value.
  stream.span()[15] = 0;
  ASSERT_TRUE(stream.seek(0));
  EXPECT_FALSE(stream.read(result));
  EXPECT_EQ(result, moon);
}

std::expected<CelestialParameters, StarException> parameters_for(Json parameters) {
  return CelestialParameters::create(JsonObject{{"coordinate", Json()}, {"name", "sample"},
      {"seed", 987654321}, {"parameters", std::move(parameters)}, {"visitableParameters", Json()}});
}

void expect_sample(std::expected<Json, StarException> const& actual, Json const& expected) {
  ASSERT_TRUE(actual) << actual.error().what();
  EXPECT_EQ(actual->type(), expected.type());
  EXPECT_EQ(*actual, expected);
}

TEST(CelestialParameters, KeepsLegacyNamedAndUnnamedSeededChoices) {
  JsonArray const range{-15, 21}, floating{1.0, 9.0};
  JsonArray const full{std::numeric_limits<std::int64_t>::min(), std::numeric_limits<std::int64_t>::max()};
  auto values = parameters_for(JsonObject{{"range", range}, {"float", floating}, {"full", full},
      {"list", JsonArray{"a", "b", "c", "d", "e", "f", "g"}}});
  ASSERT_TRUE(values) << values.error().what();
  // Frozen old hash inputs: signed int32 mix, uint64 seed, then name bytes.
  expect_sample(values->randomize_parameter_list("list", -17), "f");
  expect_sample(values->randomize_parameter_range("range", -17), 2);
  expect_sample(values->randomize_parameter_range(range, -17, "range"), 2);
  expect_sample(values->randomize_parameter_range(range, -17), 19);
  expect_sample(values->randomize_parameter_range("float", -17), 0x1.dcab1a07c6d25p+1);
  expect_sample(values->randomize_parameter_range(floating, -17), 0x1.8e415bd6161d4p+0);
  expect_sample(values->randomize_parameter_range("full", -17), -7607046758783600576LL);
  expect_sample(values->randomize_parameter_range(full, -17), 640660950120256621LL);
  expect_sample(values->randomize_parameter_range(JsonArray{-4, -4}), -4);
  expect_sample(values->randomize_parameter_range(JsonArray{1.5, 1.5}), 1.5);
  expect_sample(values->randomize_parameter_range(JsonArray{-4, -3.75}, -17), -3);
  expect_sample(values->randomize_parameter_list("missing"), Json());
  expect_sample(values->randomize_parameter_range("missing"), Json());
}

TEST(CelestialParameters, BothRangeEntriesRejectMalformedBounds) {
  volatile std::uint64_t infinity_bits = 0x7ff0000000000000ull;
  auto infinity = std::bit_cast<double>(static_cast<std::uint64_t>(infinity_bits));
  int sample = 0;
  for (JsonArray const& range : {JsonArray{}, JsonArray{1}, JsonArray{1, 2, 3}, JsonArray{0, -1},
           JsonArray{3.5, 2.5}, JsonArray{infinity, 4.0},
           JsonArray{0, -0.5}, JsonArray{0x1p63, std::numeric_limits<std::int64_t>::max()},
           JsonArray{1, "bad"}}) {
    SCOPED_TRACE(sample++);
    auto values = parameters_for(JsonObject{{"range", range}});
    ASSERT_TRUE(values) << values.error().what();
    EXPECT_FALSE(values->randomize_parameter_range("range"));
    EXPECT_FALSE(values->randomize_parameter_range(range));
  }
  auto values = parameters_for(JsonObject{{"empty", JsonArray{}}, {"scalar", true}});
  ASSERT_TRUE(values) << values.error().what();
  EXPECT_FALSE(values->randomize_parameter_range("scalar"));
  EXPECT_FALSE(values->randomize_parameter_list("scalar"));
  EXPECT_FALSE(values->randomize_parameter_list("empty"));
}

}
