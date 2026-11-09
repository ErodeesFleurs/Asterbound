import std;
import star.format;
import star.hash;
import star.xx_hash;
import star.json;
import star.string;
import star.data_stream_devices;

#include "gtest/gtest.h"

using namespace Star;

namespace {
template <typename Owner>
concept borrows_temporary_json = requires(Owner&& owner) { std::move(owner).find(std::size_t{0}); };
static_assert(!borrows_temporary_json<Json>);
static_assert(!borrows_temporary_json<Json const>);
}// namespace

TEST(JsonContractTest, EqualNumericKeysShareHashesRecursively) {
  for (auto const& [integer, floating] : std::array<std::pair<Json, Json>, 7>{
         {{Json(1), Json(1.0)},
          {Json(0), Json(-0.0)},
          {Json(0.0), Json(-0.0)},
          {Json(std::numeric_limits<std::int64_t>::min()), Json(-0x1p63)},
          {Json(std::int64_t{9007199254740992}), Json(0x1p53)},
          {Json(JsonArray{1, JsonObject{{"level", 0}}}), Json(JsonArray{1.0, JsonObject{{"level", -0.0}}})},
          {Json(JsonObject{{"level", 1}, {"fraction", 1.25}}),
           Json(JsonObject{{"fraction", 1.25}, {"level", 1.0}})}}}) {
    ASSERT_EQ(integer, floating);
    EXPECT_EQ(Star::hash<Json>()(integer), Star::hash<Json>()(floating));
    std::unordered_map<Json, int, Star::hash<Json>> values;
    values.emplace(integer, 42);
    auto found = values.find(floating);
    ASSERT_NE(found, values.end());
    EXPECT_EQ(found->second, 42);
    values.insert_or_assign(floating, 43);
    EXPECT_EQ(values.size(), 1u);
    EXPECT_EQ(values.find(integer)->second, 43);
  }
}

TEST(JsonContractTest, NumericHashKeepsLargeIntegersDistinctFromRoundedFloats) {
  Json exact(std::int64_t{9007199254740992});
  Json adjacent(std::int64_t{9007199254740993});
  Json rounded(0x1p53);
  EXPECT_EQ(exact, rounded);
  EXPECT_NE(adjacent, rounded);
  EXPECT_NE(Json(std::numeric_limits<std::int64_t>::max()), Json(0x1p63));

  std::unordered_map<Json, int, Star::hash<Json>> values;
  values.emplace(exact, 1);
  values.emplace(adjacent, 2);
  values.emplace(Json(std::numeric_limits<std::int64_t>::max()), 3);
  values.emplace(Json(0x1p63), 4);
  ASSERT_EQ(values.size(), 4u);
  auto first = values.find(rounded);
  ASSERT_NE(first, values.end());
  EXPECT_EQ(first->second, 1);
  auto second = values.find(adjacent);
  ASSERT_NE(second, values.end());
  EXPECT_EQ(second->second, 2);
  auto outside = values.find(Json(0x1p63));
  ASSERT_NE(outside, values.end());
  EXPECT_EQ(outside->second, 4);
}

TEST(JsonContractTest, UnchangedNumericHashDomainsKeepNativeBytes) {
  // Integer and nonintegral floating hashes retain their pre-cutover domain
  // byte and native scalar bytes; normalization changes only integral floats.
  for (auto integer : {std::int64_t{0}, std::int64_t{-17}, std::numeric_limits<std::int64_t>::min(),
                       std::numeric_limits<std::int64_t>::max()}) {
    XXHash3 historical;
    char tag = 3;
    historical.push(std::span<char const>(&tag, 1));
    historical.push(std::span<char const>(reinterpret_cast<char const*>(&integer), sizeof(integer)));
    EXPECT_EQ(Star::hash<Json>()(Json(integer)), historical.digest());
  }
  for (double floating : {0.5, -1.25, 0x1p63, std::numeric_limits<double>::denorm_min()}) {
    XXHash3 historical;
    char tag = 1;
    historical.push(std::span<char const>(&tag, 1));
    historical.push(std::span<char const>(reinterpret_cast<char const*>(&floating), sizeof(floating)));
    EXPECT_EQ(Star::hash<Json>()(Json(floating)), historical.digest());
  }
}

TEST(JsonContractTest, ObjectHashFastPathsPreserveHistoricalStreamAndEquivalentKeys) {
  // Build the historical stream explicitly: object tag, sorted UTF-8 keys,
  // then recursive value bytes, with no member counts or key lengths.
  auto object_bytes = [] { return std::string(1, static_cast<char>(Json::Type::Object)); };
  auto integer_bytes = [](std::int64_t value) {
    std::string bytes(1, static_cast<char>(Json::Type::Int));
    bytes.append(reinterpret_cast<char const*>(&value), sizeof(value));
    return bytes;
  };
  auto empty_bytes = object_bytes();
  auto singleton_bytes = object_bytes() + "value" + integer_bytes(0);
  auto nested_bytes = object_bytes() + "child" + singleton_bytes;
  auto multi_bytes = object_bytes() + "a" + nested_bytes + "empty" + empty_bytes + "z" + integer_bytes(1);

  struct Case {
    Json stored;
    Json equivalent;
    std::string historical_bytes;
  };
  for (auto const& entry : std::array<Case, 4>{
         {{JsonObject{}, JsonObject{}, empty_bytes},
          {JsonObject{{"value", 0}}, JsonObject{{"value", -0.0}}, singleton_bytes},
          {JsonObject{{"child", JsonObject{{"value", 0}}}}, JsonObject{{"child", JsonObject{{"value", 0.0}}}},
           nested_bytes},
          {JsonObject{{"z", 1}, {"empty", JsonObject{}}, {"a", JsonObject{{"child", JsonObject{{"value", 0}}}}}},
           JsonObject{{"a", JsonObject{{"child", JsonObject{{"value", -0.0}}}}}, {"empty", JsonObject{}}, {"z", 1.0}},
           multi_bytes}}}) {
    ASSERT_EQ(entry.stored, entry.equivalent);
    XXHash3 historical;
    historical.push(std::span<char const>(entry.historical_bytes));
    EXPECT_EQ(Star::hash<Json>()(entry.stored), historical.digest());
    EXPECT_EQ(Star::hash<Json>()(entry.equivalent), historical.digest());

    std::unordered_map<Json, int, Star::hash<Json>> values;
    values.emplace(entry.stored, 42);
    auto found = values.find(entry.equivalent);
    ASSERT_NE(found, values.end());
    EXPECT_EQ(found->second, 42);
    values.insert_or_assign(entry.equivalent, 43);
    EXPECT_EQ(values.size(), 1u);
    auto updated = values.find(entry.stored);
    ASSERT_NE(updated, values.end());
    EXPECT_EQ(updated->second, 43);
  }
}

TEST(JsonContractTest, BorrowedLookupDistinguishesMissingNullAndWrongContainer) {
  Json object = JsonObject{{"null", Json()}, {"child", JsonObject{{"value", 7}}}};
  auto missing = object.find("missing");
  ASSERT_TRUE(missing);
  EXPECT_FALSE(*missing);
  auto null = object.find("null");
  ASSERT_TRUE(null);
  ASSERT_TRUE(*null);
  EXPECT_TRUE((*null)->get().isNull());
  auto child = object.find("child");
  ASSERT_TRUE(child);
  ASSERT_TRUE(*child);

  auto changed = object.set("child", 99);
  ASSERT_TRUE(changed);
  auto originalValue = (*child)->get().getInt("value");
  ASSERT_TRUE(originalValue);
  EXPECT_EQ(*originalValue, 7);
  auto changedValue = changed->getInt("child");
  ASSERT_TRUE(changedValue);
  EXPECT_EQ(*changedValue, 99);

  Json array = JsonArray{Json(), 8};
  auto first = array.find(std::size_t{0});
  ASSERT_TRUE(first);
  ASSERT_TRUE(*first);
  EXPECT_TRUE((*first)->get().isNull());
  auto outside = array.find(std::size_t{2});
  ASSERT_TRUE(outside);
  EXPECT_FALSE(*outside);
  auto wrongIndex = object.find(std::size_t{0});
  ASSERT_FALSE(wrongIndex);
  EXPECT_TRUE(wrongIndex.error().is<JsonException>());
  auto wrongKey = array.find("value");
  ASSERT_FALSE(wrongKey);
  EXPECT_TRUE(wrongKey.error().is<JsonException>());
}

TEST(JsonContractTest, DefaultsPreserveKeyIndexAndUntypedNullPolicies) {
  Json object = JsonObject{{"null", Json()}, {"wrong", "text"}};
  Json array = JsonArray{Json(), "text"};
  auto keyed = object.getInt("null", 5);
  ASSERT_TRUE(keyed);
  EXPECT_EQ(*keyed, 5);
  auto missing = object.getInt("missing", 5);
  ASSERT_TRUE(missing);
  EXPECT_EQ(*missing, 5);
  auto indexed = array.getInt(std::size_t{0}, 5);
  ASSERT_FALSE(indexed);
  EXPECT_TRUE(indexed.error().is<JsonException>());
  auto outside = array.getInt(std::size_t{2}, 5);
  ASSERT_TRUE(outside);
  EXPECT_EQ(*outside, 5);
  auto wrongKey = object.getInt("wrong", 5);
  ASSERT_FALSE(wrongKey);
  auto wrongIndex = array.getInt(std::size_t{1}, 5);
  ASSERT_FALSE(wrongIndex);

  auto rawKey = object.get("null", 5);
  ASSERT_TRUE(rawKey);
  EXPECT_TRUE(rawKey->isNull());
  auto rawIndex = array.get(std::size_t{0}, 5);
  ASSERT_TRUE(rawIndex);
  EXPECT_TRUE(rawIndex->isNull());
  auto optional = object.optInt("null");
  ASSERT_TRUE(optional);
  EXPECT_FALSE(*optional);
  auto absentOptional = object.optInt("missing");
  ASSERT_TRUE(absentOptional);
  EXPECT_FALSE(*absentOptional);
  auto invalidOptional = object.optInt("wrong");
  ASSERT_FALSE(invalidOptional);
  auto scalar = Json(1).getInt(std::size_t{0}, 5);
  ASSERT_FALSE(scalar);
}

TEST(JsonContractTest, TypedQueriesPreserveNullDefaultsAndConversionErrors) {
  Json owner = JsonObject{{"array", JsonArray{Json(), 12}}, {"wrong", false}};
  auto nullDefault = owner.queryInt("array[0]", 5);
  ASSERT_TRUE(nullDefault);
  EXPECT_EQ(*nullDefault, 5);
  auto missingDefault = owner.queryInt("array[8]", 5);
  ASSERT_TRUE(missingDefault);
  EXPECT_EQ(*missingDefault, 5);
  auto value = owner.queryInt("array[1]");
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, 12);
  auto nullRequired = owner.queryInt("array[0]");
  ASSERT_FALSE(nullRequired);
  auto rawNull = owner.query("array[0]", 5);
  ASSERT_TRUE(rawNull);
  EXPECT_TRUE(rawNull->isNull());
  auto wrong = owner.queryInt("wrong", 5);
  ASSERT_FALSE(wrong);
  auto optionalNull = owner.optQueryInt("array[0]");
  ASSERT_TRUE(optionalNull);
  EXPECT_FALSE(*optionalNull);
  auto optionalMissing = owner.optQueryInt("absent");
  ASSERT_TRUE(optionalMissing);
  EXPECT_FALSE(*optionalMissing);
  auto optionalWrong = owner.optQueryInt("wrong");
  ASSERT_FALSE(optionalWrong);
}

TEST(JsonContractTest, NumericHashNormalizationDoesNotNormalizeWireTypes) {
  for (auto const& [value, tag] :
       std::array<std::pair<Json, unsigned char>, 7>{{{Json(), 1},
                                                      {Json(1.0), 2},
                                                      {Json(true), 3},
                                                      {Json(1), 4},
                                                      {Json("text"), 5},
                                                      {Json(JsonArray{1}), 6},
                                                      {Json(JsonObject{{"level", 1}}), 7}}}) {
    DataStreamBuffer stream;
    auto written = stream << value;
    ASSERT_TRUE(written);
    ASSERT_GT(stream.size(), 0u);
    EXPECT_EQ(static_cast<unsigned char>(stream.span()[0]), tag);
    auto sought = stream.seek(0);
    ASSERT_TRUE(sought);
    Json restored;
    auto decoded = stream >> restored;
    ASSERT_TRUE(decoded);
    EXPECT_EQ(restored.type(), value.type());
    EXPECT_EQ(restored, value);
  }
}

TEST(JsonContractTest, PartialMatchPreservesSharedObjectsAndArrayPatternDirection) {
  Json shared = JsonObject{{"value", std::numeric_limits<double>::quiet_NaN()}};
  Json snapshot = shared;
  EXPECT_TRUE(jsonPartialMatch(shared, snapshot));
  EXPECT_TRUE(jsonPartialMatch(shared, JsonObject{}));

  Json subset = JsonObject{{"id", 1}};
  Json superset = JsonObject{{"id", 1}, {"extra", 2}};
  EXPECT_TRUE(jsonPartialMatch(superset, subset));
  EXPECT_FALSE(jsonPartialMatch(subset, superset));
  EXPECT_TRUE(jsonPartialMatch(JsonArray{subset}, JsonArray{superset}));
  EXPECT_FALSE(jsonPartialMatch(JsonArray{superset}, JsonArray{subset}));
  EXPECT_TRUE(jsonPartialMatch(JsonArray{1, 2, 1}, JsonArray{1, 2, 1}));
}

TEST(JsonContractTest, BinaryStringPreservesLongEmbeddedNullAndCommitsOnlyCompletePayload) {
  std::string bytes(4096, 'x');
  bytes[127] = '\0';
  String text(std::move(bytes));
  Json value(text);
  auto encoded = DataStreamBuffer::serialize(value);
  ASSERT_TRUE(encoded);
  DataStreamBuffer expected;
  ASSERT_TRUE(expected.write<std::uint8_t>(5));
  ASSERT_TRUE(expected.write(text));
  EXPECT_EQ(*encoded, expected.data());

  Json decoded;
  ASSERT_TRUE(DataStreamBuffer::deserialize(decoded, encoded->span()));
  EXPECT_EQ(decoded, value);
  encoded->resize(encoded->size() - 1);
  Json retained = JsonObject{{"keep", 17}};
  auto failed = DataStreamBuffer::deserialize(retained, encoded->span());
  ASSERT_FALSE(failed);
  EXPECT_EQ(retained, Json(JsonObject{{"keep", 17}}));
}
