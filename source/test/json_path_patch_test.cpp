import std;
import star.json;
import star.json_path;
import star.json_patch;
import star.string;

#include "gtest/gtest.h"

using namespace Star;

namespace {
template <typename T>
concept can_borrow_json_path =
  requires(T&& value) { JsonPath::pathGetRef(std::forward<T>(value), JsonPath::parsePointer, String("/v")); };
static_assert(can_borrow_json_path<Json&>);
static_assert(can_borrow_json_path<Json const&>);
static_assert(!can_borrow_json_path<Json>);
static_assert(!can_borrow_json_path<Json const>);
}// namespace

TEST(JsonPathPatchTest, SearchMutationsKeepFirstMatchAndSnapshots) {
  Json first = JsonObject{{"id", "a"}, {"count", 1}};
  Json second = JsonObject{{"id", "b"}, {"count", 2}};
  Json third = JsonObject{{"id", "a"}, {"count", 3}};
  Json original = JsonObject{{"root", JsonObject{{"items", JsonArray{first, second, third}}, {"other", 17}}}};
  Json snapshot = original;
  Json search = JsonObject{{"id", "a"}};
  Json replacement = JsonObject{{"id", "new"}};
  auto apply = [&](String const& operation, Json value) {
    return jsonPatch(
      original,
      JsonArray{
        JsonObject{{"op", operation}, {"path", "/root/items"}, {"search", search}, {"value", std::move(value)}}});
  };
  auto removed = apply("remove", Json());
  ASSERT_TRUE(removed);
  auto removedItems = removed->query("root.items");
  ASSERT_TRUE(removedItems);
  EXPECT_EQ(*removedItems, Json(JsonArray{second, third}));
  auto added = apply("add", replacement);
  ASSERT_TRUE(added);
  auto addedItems = added->query("root.items");
  ASSERT_TRUE(addedItems);
  EXPECT_EQ(*addedItems, Json(JsonArray{replacement, first, second, third}));
  auto replaced = apply("replace", replacement);
  ASSERT_TRUE(replaced);
  auto replacedItems = replaced->query("root.items");
  ASSERT_TRUE(replacedItems);
  EXPECT_EQ(*replacedItems, Json(JsonArray{replacement, second, third}));
  auto merged = apply("merge", JsonObject{{"extra", 9}});
  ASSERT_TRUE(merged);
  auto mergedItems = merged->query("root.items");
  ASSERT_TRUE(mergedItems);
  EXPECT_EQ(*mergedItems, Json(JsonArray{JsonObject{{"id", "a"}, {"count", 1}, {"extra", 9}}, second, third}));
  auto sibling = merged->queryInt("root.other");
  ASSERT_TRUE(sibling);
  EXPECT_EQ(*sibling, 17);
  EXPECT_EQ(original, snapshot);
  auto originalItems = snapshot.query("root.items");
  ASSERT_TRUE(originalItems);
  EXPECT_EQ(*originalItems, Json(JsonArray{first, second, third}));
}

TEST(JsonPathPatchTest, SearchMergeOnlyValidatesValueAfterMatch) {
  Json base = JsonObject{{"items", JsonArray{JsonObject{{"id", "a"}}}}};
  Json operation = JsonObject{{"op", "merge"}, {"path", "/items"}, {"search", JsonObject{{"id", "missing"}}}};
  auto unmatched = jsonPatch(base, JsonArray{operation});
  ASSERT_TRUE(unmatched);
  EXPECT_EQ(*unmatched, base);
  auto matching = operation.set("search", JsonObject{{"id", "a"}});
  ASSERT_TRUE(matching);
  auto invalid = jsonPatch(base, JsonArray{*matching});
  ASSERT_FALSE(invalid);
  EXPECT_TRUE(invalid.error().is<JsonPatchException>());
  auto nonArray = jsonPatch(JsonObject{{"items", 3}}, JsonArray{operation});
  ASSERT_FALSE(nonArray);
  EXPECT_TRUE(nonArray.error().is<JsonPatchException>());
}

TEST(JsonPathPatchTest, ReplaceRequiresExistingLocationButAllowsNullAndRoot) {
  Json base = JsonObject{{"items", JsonArray{Json(), 2}}, {"null", Json()}};
  for (std::string_view path : std::array<std::string_view, 3>{"/missing", "/items/2", "/items/-"}) {
    auto replaced = jsonPatch(base, JsonArray{JsonObject{{"op", "replace"}, {"path", Json(path)}, {"value", 7}}});
    ASSERT_FALSE(replaced);
    EXPECT_TRUE(replaced.error().is<JsonPatchException>());
  }
  auto replaced = jsonPatch(base, JsonArray{JsonObject{{"op", "replace"}, {"path", "/items/0"}, {"value", 7}}});
  ASSERT_TRUE(replaced);
  auto items = replaced->get("items");
  ASSERT_TRUE(items);
  EXPECT_EQ(*items, Json(JsonArray{7, 2}));
  auto objectNull = jsonPatch(base, JsonArray{JsonObject{{"op", "replace"}, {"path", "/null"}, {"value", 8}}});
  ASSERT_TRUE(objectNull);
  auto value = objectNull->getInt("null");
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, 8);
  auto root = jsonPatch(base, JsonArray{JsonObject{{"op", "replace"}, {"path", ""}, {"value", JsonArray{9}}}});
  ASSERT_TRUE(root);
  EXPECT_EQ(*root, Json(JsonArray{9}));
}

TEST(JsonPathPatchTest, MergePreservesRootAndStrictTargetRules) {
  Json base = JsonObject{{"nested", JsonObject{{"keep", 1}, {"change", 2}}}};
  auto merged =
    jsonPatch(base, JsonArray{JsonObject{{"op", "merge"}, {"path", "/nested"}, {"value", JsonObject{{"change", 3}}}}});
  ASSERT_TRUE(merged);
  EXPECT_EQ(*merged, Json(JsonObject{{"nested", JsonObject{{"keep", 1}, {"change", 3}}}}));
  auto missing = jsonPatch(base, JsonArray{JsonObject{{"op", "merge"}, {"path", "/missing"}, {"value", JsonObject{}}}});
  ASSERT_FALSE(missing);
  auto root = jsonPatch(Json(), JsonArray{JsonObject{{"op", "merge"}, {"path", ""}, {"value", JsonObject{{"v", 4}}}}});
  ASSERT_TRUE(root);
  EXPECT_EQ(*root, Json(JsonObject{{"v", 4}}));
}

TEST(JsonPathPatchTest, BorrowedLookupKeepsNullAndLazyErrorPrecedence) {
  Json base = JsonObject{{"value", Json()}, {"scalar", 1}, {"/", JsonObject{{"~", JsonObject{{"", 2}}}}}};
  auto nullValue = JsonPath::pathFindRef(base, JsonPath::parsePointer, "/value");
  ASSERT_TRUE(nullValue);
  ASSERT_TRUE(*nullValue);
  EXPECT_TRUE((*nullValue)->get().isNull());
  auto missing = JsonPath::pathFindRef(base, JsonPath::parsePointer, "/missing/~2");
  ASSERT_TRUE(missing);
  EXPECT_FALSE(*missing);
  auto traversal = JsonPath::pathGetRef(base, JsonPath::parsePointer, "/missing/~2");
  ASSERT_FALSE(traversal);
  EXPECT_TRUE(traversal.error().is<JsonPath::TraversalException>());
  auto malformed = JsonPath::pathFindRef(base, JsonPath::parsePointer, "/~2/missing");
  ASSERT_FALSE(malformed);
  EXPECT_TRUE(malformed.error().is<JsonPath::ParsingException>());
  auto scalar = JsonPath::pathFindRef(base, JsonPath::parsePointer, "/scalar/child/~2");
  ASSERT_TRUE(scalar);
  EXPECT_FALSE(*scalar);
  auto escaped = JsonPath::pathGetRef(base, JsonPath::parsePointer, "/~1/~0/");
  ASSERT_TRUE(escaped);
  EXPECT_EQ(escaped->get(), Json(2));
  auto updated = JsonPath::Pointer("/~1/~0/").set(base, Json(3));
  ASSERT_TRUE(updated);
  auto changed = JsonPath::pathGetRef(*updated, JsonPath::parsePointer, "/~1/~0/");
  ASSERT_TRUE(changed);
  EXPECT_EQ(changed->get(), Json(3));
  EXPECT_EQ(escaped->get(), Json(2));
}

TEST(JsonPathPatchTest, ApplyKeepsLazyErrorsAndBorrowedSnapshots) {
  Json base = JsonObject{{"items", JsonArray{JsonObject{{"value", 1}}, Json()}}, {"scalar", 3}};
  auto snapshot = JsonPath::pathGetRef(base, JsonPath::parsePointer, "/items/0");
  ASSERT_TRUE(snapshot);
  for (std::string_view path : std::array<std::string_view, 3>{"/missing/~2", "/items/2/~2", "/scalar/child/~2"}) {
    auto changed = JsonPath::Pointer(String(path)).set(base, Json(7));
    ASSERT_FALSE(changed);
    EXPECT_TRUE(changed.error().is<JsonPath::TraversalException>());
  }
  auto malformed = JsonPath::Pointer("/~2/missing").set(base, Json(7));
  ASSERT_FALSE(malformed);
  EXPECT_TRUE(malformed.error().is<JsonPath::ParsingException>());
  auto changed = JsonPath::QueryPath("items[0].value").set(base, Json(7));
  ASSERT_TRUE(changed);
  auto value = changed->queryInt("items[0].value");
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, 7);
  EXPECT_EQ(snapshot->get(), Json(JsonObject{{"value", 1}}));
  auto originalValue = base.queryInt("items[0].value");
  ASSERT_TRUE(originalValue);
  EXPECT_EQ(*originalValue, 1);
}

TEST(JsonPathPatchTest, MergeChecksTargetBeforeValueAndKeepsUnreachableSuffixLazy) {
  Json base = JsonObject{{"nested", JsonObject{{"value", 1}}}, {"items", JsonArray{Json()}}};
  for (std::string_view path :
       std::array<std::string_view, 5>{"/missing", "/items/1", "/items/-", "/missing/~2", "/items/1/~2"}) {
    auto merged = JsonPatching::applyMergeOperation(base, JsonObject{{"path", Json(path)}});
    ASSERT_FALSE(merged);
    EXPECT_TRUE(merged.error().is<JsonPath::TraversalException>());
  }
  auto malformed = JsonPatching::applyMergeOperation(base, JsonObject{{"path", "/~2/missing"}});
  ASSERT_FALSE(malformed);
  EXPECT_TRUE(malformed.error().is<JsonPath::ParsingException>());
  for (std::string_view path : std::array<std::string_view, 3>{"", "/nested", "/items/0"}) {
    auto missingValue = JsonPatching::applyMergeOperation(base, JsonObject{{"path", Json(path)}});
    ASSERT_FALSE(missingValue);
    EXPECT_TRUE(missingValue.error().is<JsonException>());
    EXPECT_FALSE(missingValue.error().is<JsonPath::TraversalException>());
    EXPECT_FALSE(missingValue.error().is<JsonPath::ParsingException>());
  }
}

TEST(JsonPathPatchTest, RepeatedMergeKeepsEarlierResultAndChildSnapshots) {
  Json base = JsonObject{{"root", JsonObject{{"target", JsonObject{{"keep", 1}, {"value", 2}}}, {"sibling", 3}}}};
  auto child = JsonPath::pathGetRef(base, JsonPath::parsePointer, "/root/target");
  ASSERT_TRUE(child);
  auto first = jsonPatch(
    base, JsonArray{JsonObject{{"op", "merge"}, {"path", "/root/target"}, {"value", JsonObject{{"value", 4}}}}});
  ASSERT_TRUE(first);
  auto second = jsonPatch(
    *first, JsonArray{JsonObject{{"op", "merge"}, {"path", "/root/target"}, {"value", JsonObject{{"added", 5}}}}});
  ASSERT_TRUE(second);
  EXPECT_EQ(child->get(), Json(JsonObject{{"keep", 1}, {"value", 2}}));
  auto firstTarget = first->query("root.target");
  ASSERT_TRUE(firstTarget);
  EXPECT_EQ(*firstTarget, Json(JsonObject{{"keep", 1}, {"value", 4}}));
  EXPECT_EQ(*second,
            Json(JsonObject{
              {"root", JsonObject{{"target", JsonObject{{"keep", 1}, {"value", 4}, {"added", 5}}}, {"sibling", 3}}}}));
}
