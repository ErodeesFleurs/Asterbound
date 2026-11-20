import std;
import star.format;
import star.hash;
import star.xx_hash;
import star.json;
import star.json_builder;
import star.json_extra;
import star.list;
import star.vector;
import star.string;
import star.data_stream_devices;
import star.text_formatter;
import star.unicode;

#include "gtest/gtest.h"

using namespace Star;

namespace {
template <typename Owner>
concept borrows_temporary_json = requires(Owner&& owner) { std::move(owner).find(std::size_t{0}); };
static_assert(!borrows_temporary_json<Json>);
static_assert(!borrows_temporary_json<Json const>);

void expect_equivalent_key_lookup(Json const& stored, Json const& equivalent) {
  ASSERT_EQ(stored, equivalent);
  EXPECT_EQ(Star::hash<Json>()(stored), Star::hash<Json>()(equivalent));
  std::unordered_map<Json, int, Star::hash<Json>> values;
  values.emplace(stored, 42);
  auto found = values.find(equivalent);
  ASSERT_NE(found, values.end());
  EXPECT_EQ(found->second, 42);
  values.insert_or_assign(equivalent, 43);
  EXPECT_EQ(values.size(), 1u);
  auto updated = values.find(stored);
  ASSERT_NE(updated, values.end());
  EXPECT_EQ(updated->second, 43);
}
}// namespace

TEST(JsonContractTest, VectorConversionChecksShapeBeforeFirstInvalidComponent) {
  auto wrong_shape = json_to_vec2f(JsonArray{JsonObject()});
  ASSERT_FALSE(wrong_shape);
  EXPECT_NE(std::string_view(wrong_shape.error().what()).find("array of size 2"), std::string_view::npos);
  auto wrong_component = json_to_vec2f(JsonArray{"first", JsonObject()});
  ASSERT_FALSE(wrong_component);
  EXPECT_NE(std::string_view(wrong_component.error().what()).find("string"), std::string_view::npos);
  auto valid = json_to_vec2f(JsonArray{2, -0.25});
  ASSERT_TRUE(valid);
  EXPECT_EQ((*valid)[0], 2.0f);
  EXPECT_EQ((*valid)[1], -0.25f);
}

TEST(JsonContractTest, ListConversionUsesOneMutableLvalueConverter) {
  struct Converter {
    int total = 0;
    std::expected<int, StarException> operator()(Json const& value) & {
      auto number = value.to_int();
      if (!number)
        return std::unexpected(std::move(number.error()));
      return total += static_cast<int>(*number);
    }
  };
  auto result = json_to_list<int>(JsonArray{2, 3, 4}, Converter{});
  ASSERT_TRUE(result);
  EXPECT_EQ(*result, (List<int>{2, 5, 9}));
}

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
    expect_equivalent_key_lookup(integer, floating);
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
    expect_equivalent_key_lookup(entry.stored, entry.equivalent);
    XXHash3 historical;
    historical.push(std::span<char const>(entry.historical_bytes));
    EXPECT_EQ(Star::hash<Json>()(entry.stored), historical.digest());
    EXPECT_EQ(Star::hash<Json>()(entry.equivalent), historical.digest());

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
  EXPECT_TRUE((*null)->get().is_null());
  auto child = object.find("child");
  ASSERT_TRUE(child);
  ASSERT_TRUE(*child);

  auto changed = object.set("child", 99);
  ASSERT_TRUE(changed);
  auto originalValue = (*child)->get().get_int("value");
  ASSERT_TRUE(originalValue);
  EXPECT_EQ(*originalValue, 7);
  auto changedValue = changed->get_int("child");
  ASSERT_TRUE(changedValue);
  EXPECT_EQ(*changedValue, 99);

  Json array = JsonArray{Json(), 8};
  auto first = array.find(std::size_t{0});
  ASSERT_TRUE(first);
  ASSERT_TRUE(*first);
  EXPECT_TRUE((*first)->get().is_null());
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
  auto keyed = object.get_int("null", 5);
  ASSERT_TRUE(keyed);
  EXPECT_EQ(*keyed, 5);
  auto missing = object.get_int("missing", 5);
  ASSERT_TRUE(missing);
  EXPECT_EQ(*missing, 5);
  auto indexed = array.get_int(std::size_t{0}, 5);
  ASSERT_FALSE(indexed);
  EXPECT_TRUE(indexed.error().is<JsonException>());
  auto outside = array.get_int(std::size_t{2}, 5);
  ASSERT_TRUE(outside);
  EXPECT_EQ(*outside, 5);
  auto outside_without_default = array.get_int(std::size_t{2});
  ASSERT_FALSE(outside_without_default);
  EXPECT_TRUE(outside_without_default.error().is<JsonException>());
  auto wrongKey = object.get_int("wrong", 5);
  ASSERT_FALSE(wrongKey);
  auto wrongIndex = array.get_int(std::size_t{1}, 5);
  ASSERT_FALSE(wrongIndex);

  auto rawKey = object.get("null", 5);
  ASSERT_TRUE(rawKey);
  EXPECT_TRUE(rawKey->is_null());
  auto rawIndex = array.get(std::size_t{0}, 5);
  ASSERT_TRUE(rawIndex);
  EXPECT_TRUE(rawIndex->is_null());
  auto optional = object.opt_int("null");
  ASSERT_TRUE(optional);
  EXPECT_FALSE(*optional);
  auto absentOptional = object.opt_int("missing");
  ASSERT_TRUE(absentOptional);
  EXPECT_FALSE(*absentOptional);
  auto invalidOptional = object.opt_int("wrong");
  ASSERT_FALSE(invalidOptional);
  auto scalar = Json(1).get_int(std::size_t{0}, 5);
  ASSERT_FALSE(scalar);
}

TEST(JsonContractTest, TypedQueriesPreserveNullDefaultsAndConversionErrors) {
  Json owner = JsonObject{{"array", JsonArray{Json(), 12}}, {"wrong", false}};
  auto nullDefault = owner.query_int("array[0]", 5);
  ASSERT_TRUE(nullDefault);
  EXPECT_EQ(*nullDefault, 5);
  auto missingDefault = owner.query_int("array[8]", 5);
  ASSERT_TRUE(missingDefault);
  EXPECT_EQ(*missingDefault, 5);
  auto value = owner.query_int("array[1]");
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, 12);
  auto nullRequired = owner.query_int("array[0]");
  ASSERT_FALSE(nullRequired);
  auto rawNull = owner.query("array[0]", 5);
  ASSERT_TRUE(rawNull);
  EXPECT_TRUE(rawNull->is_null());
  auto wrong = owner.query_int("wrong", 5);
  ASSERT_FALSE(wrong);
  auto optionalNull = owner.opt_query_int("array[0]");
  ASSERT_TRUE(optionalNull);
  EXPECT_FALSE(*optionalNull);
  auto optionalMissing = owner.opt_query_int("absent");
  ASSERT_TRUE(optionalMissing);
  EXPECT_FALSE(*optionalMissing);
  auto optionalWrong = owner.opt_query_int("wrong");
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

TEST(JsonContractTest, RecursiveMergeDistinguishesNullFromDeletion) {
  Json base = JsonObject{{"nested", JsonObject{{"keep", 1}, {"remove", 2}}}};
  Json patch = JsonObject{{"nested", JsonObject{{"remove", Json()}}}};
  EXPECT_EQ(json_merge(base, patch), base);
  EXPECT_EQ(json_merge_nulling(base, patch), (JsonObject{{"nested", JsonObject{{"keep", 1}}}}));
}

TEST(JsonContractTest, PartialMatchPreservesSharedObjectsAndArrayPatternDirection) {
  Json shared = JsonObject{{"value", std::numeric_limits<double>::quiet_NaN()}};
  Json snapshot = shared;
  EXPECT_TRUE(json_partial_match(shared, snapshot));
  EXPECT_TRUE(json_partial_match(shared, JsonObject{}));

  Json subset = JsonObject{{"id", 1}};
  Json superset = JsonObject{{"id", 1}, {"extra", 2}};
  EXPECT_TRUE(json_partial_match(superset, subset));
  EXPECT_FALSE(json_partial_match(subset, superset));
  EXPECT_TRUE(json_partial_match(JsonArray{subset}, JsonArray{superset}));
  EXPECT_FALSE(json_partial_match(JsonArray{superset}, JsonArray{subset}));
  EXPECT_TRUE(json_partial_match(JsonArray{1, 2, 1}, JsonArray{1, 2, 1}));
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

TEST(JsonContractTest, ArrayMutationBoundsPreserveOriginalValues) {
  for (Json const& original : {Json(JsonArray{}), Json(JsonArray{1, Json(), 3})}) {
    auto snapshot = original;
    auto values = original.to_array();
    ASSERT_TRUE(values);
    auto count = values->size();
    for (std::size_t index : {count, count + 1, std::numeric_limits<std::size_t>::max()}) {
      auto replaced = original.set(index, 9);
      auto erased = original.erase(index);
      ASSERT_FALSE(replaced);
      ASSERT_FALSE(erased);
      EXPECT_TRUE(replaced.error().is<JsonException>());
      EXPECT_TRUE(erased.error().is<JsonException>());
      if (index != count) {
        auto inserted = original.insert(index, 9);
        ASSERT_FALSE(inserted);
        EXPECT_TRUE(inserted.error().is<JsonException>());
      }
    }
    auto inserted = original.insert(count, 9);
    auto appended = original.append(9);
    ASSERT_TRUE(inserted);
    ASSERT_TRUE(appended);
    values->push_back(9);
    EXPECT_EQ(*inserted, Json(*values));
    EXPECT_EQ(*appended, Json(*values));
    EXPECT_EQ(original, snapshot);
  }

  Json scalar = 1;
  auto replaced = scalar.set(std::size_t{0}, 9);
  auto inserted = scalar.insert(std::size_t{0}, 9);
  auto appended = scalar.append(9);
  auto erased = scalar.erase(std::size_t{0});
  ASSERT_FALSE(replaced);
  ASSERT_FALSE(inserted);
  ASSERT_FALSE(appended);
  ASSERT_FALSE(erased);
  EXPECT_TRUE(replaced.error().is<JsonException>());
  EXPECT_TRUE(inserted.error().is<JsonException>());
  EXPECT_TRUE(appended.error().is<JsonException>());
  EXPECT_TRUE(erased.error().is<JsonException>());
}

TEST(JsonContractTest, ArrayMutationsAcceptSelfAndBorrowedChildrenWithoutChangingSnapshots) {
  Json original = JsonArray{JsonObject{{"value", 7}}, Json(), 3};
  auto snapshot = original;
  auto child = original.find(std::size_t{0});
  ASSERT_TRUE(child);
  ASSERT_TRUE(*child);
  Json child_snapshot = (*child)->get();

  for (std::size_t index : {std::size_t{0}, std::size_t{1}, std::size_t{2}}) {
    auto replaced = original.set(index, original);
    auto inserted = original.insert(index, original);
    auto erased = original.erase(index);
    ASSERT_TRUE(replaced);
    ASSERT_TRUE(inserted);
    ASSERT_TRUE(erased);
    JsonArray expected{child_snapshot, Json(), 3};
    expected[index] = snapshot;
    EXPECT_EQ(*replaced, Json(expected));
    expected = JsonArray{child_snapshot, Json(), 3};
    expected.insert(expected.begin() + index, snapshot);
    EXPECT_EQ(*inserted, Json(expected));
    expected = JsonArray{child_snapshot, Json(), 3};
    expected.erase(expected.begin() + index);
    EXPECT_EQ(*erased, Json(expected));
  }
  auto appended = original.append(original);
  auto borrowed = original.insert(std::size_t{1}, (*child)->get());
  ASSERT_TRUE(appended);
  ASSERT_TRUE(borrowed);
  EXPECT_EQ(*appended, (Json(JsonArray{child_snapshot, Json(), 3, snapshot})));
  EXPECT_EQ(*borrowed, (Json(JsonArray{child_snapshot, child_snapshot, Json(), 3})));
  original = *appended;
  EXPECT_EQ(snapshot, (Json(JsonArray{child_snapshot, Json(), 3})));
  EXPECT_EQ(child_snapshot, (Json(JsonObject{{"value", 7}})));
  EXPECT_EQ(*borrowed, (Json(JsonArray{child_snapshot, child_snapshot, Json(), 3})));
}

TEST(JsonContractTest, DirectObjectTextPreservesTraversalNestedValuesAndKeyQuirks) {
  JsonObject object{{"z", JsonArray{String(std::string_view("a\0b\n", 4)), "水😀", true, Json()}},
                    {"a", JsonObject{{"number", std::numeric_limits<std::int64_t>::min()}}},
                    {"empty", JsonObject{}}};
  auto expected = Json(object).repr(0, false);
  ASSERT_TRUE(expected);
  TextBuffer output;
  output.append("prefix:");
  ASSERT_TRUE(write_text(output, object));
  EXPECT_EQ(output.view(), std::string("prefix:") + expected->utf8());
  auto parsed = Json::parse_json(*expected);
  ASSERT_TRUE(parsed);
  EXPECT_EQ(*parsed, Json(object));

  TextBuffer empty_output;
  ASSERT_TRUE(write_text(empty_output, JsonObject{}));
  EXPECT_EQ(empty_output.view(), "{}");

  String key(std::string_view("é\"key\0suffix", 13));
  TextBuffer key_output;
  ASSERT_TRUE(write_text(key_output, JsonObject{{key, String(std::string_view("a\0b\n", 4))}}));
  // Raw keys stop at NUL without escaping quotes; values remain JSON-escaped.
  EXPECT_EQ(key_output.view(), R"({"é"key":"a\u0000b\n"})");
}

TEST(JsonContractTest, DirectObjectTextValidatesEachKeyAndValueBeforeItsOwnOutput) {
  struct Case {
    JsonObject m_value;
    std::string m_prefix;
  };
  String invalid(std::string_view("bad\xff", 4));
  String invalid_key(std::string_view("key\0\xff", 5));
  for (auto const& entry :
       std::array<Case, 4>{{{JsonObject{{invalid_key, invalid}}, "prefix:{"},
                            {JsonObject{{"key", invalid}}, "prefix:{\"key\":"},
                            {JsonObject{{"key", JsonArray{1, invalid, 2}}}, "prefix:{\"key\":[1,"},
                            {JsonObject{{"key", JsonObject{{invalid_key, 1}}}}, "prefix:{\"key\":{"}}}) {
    TextBuffer output;
    output.append("prefix:");
    auto written = write_text(output, entry.m_value);
    ASSERT_FALSE(written);
    EXPECT_TRUE(written.error().is<UnicodeException>());
    EXPECT_EQ(output.view(), entry.m_prefix);
  }
}

TEST(JsonContractTest, BuilderCopyAndMoveKeepIndependentActiveNestedContainers) {
  JsonBuilderStream original;
  ASSERT_TRUE(original.begin_array());
  ASSERT_TRUE(original.put_integer("1"));
  ASSERT_TRUE(original.begin_object());
  ASSERT_TRUE(original.object_key_utf8("items"));
  ASSERT_TRUE(original.begin_array());
  ASSERT_TRUE(original.put_integer("2"));

  JsonBuilderStream copied(original);
  JsonBuilderStream moved(std::move(original));
  JsonBuilderStream assigned;
  ASSERT_TRUE(assigned.begin_object());
  assigned = std::move(moved);

  for (auto builder : std::array{std::ref(copied), std::ref(assigned)}) {
    ASSERT_TRUE(builder.get().put_integer("3"));
    ASSERT_TRUE(builder.get().end_array());
    ASSERT_TRUE(builder.get().end_object());
    ASSERT_TRUE(builder.get().end_array());
    auto result = builder.get().take_top();
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, (Json(JsonArray{1, JsonObject{{"items", JsonArray{2, 3}}}})));
  }

  for (auto builder : std::array{std::ref(original), std::ref(moved)}) {
    ASSERT_TRUE(builder.get().begin_array());
    ASSERT_TRUE(builder.get().put_null());
    ASSERT_TRUE(builder.get().end_array());
    auto result = builder.get().take_top();
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, Json(JsonArray{Json()}));
    auto incomplete = builder.get().end_array();
    ASSERT_FALSE(incomplete);
    EXPECT_TRUE(incomplete.error().is<JsonParsingException>());
    ASSERT_TRUE(builder.get().begin_array());
    ASSERT_TRUE(builder.get().put_integer("7"));
    ASSERT_TRUE(builder.get().end_array());
    auto recovered = builder.get().take_top();
    ASSERT_TRUE(recovered);
    EXPECT_EQ(*recovered, Json(JsonArray{7}));
  }
}
