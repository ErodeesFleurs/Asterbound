import star.json;
import star.set;
import star.string;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.data_stream_extra;
import star.data_stream_devices;
import star.game_timers;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.strong_typedef;
import star.bi_map;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.text_formatter;

import star.drawable;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.item;
import star.item_descriptor;
import star.item_recipe;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;


#include "gtest/gtest.h"


import star.item_database;
import star.item_bag;
import star.player_inventory;
import star.inventory_types;
import star.armors;
import star.net_element_top;
import star.net_compatibility;
import star.map;
import star.byte_array;
import star.format;
import star.game_test;
import star.algorithm;
import star.file;
import star.random;
import star.player_log;
import star.statistics;
import star.collection_database;
import star.entity;
import star.player;
import star.player_blueprints;
import star.player_factory;
import star.augment_item;
import star.object_database;
import star.damage_types;
import star.light_source;

import star.beam_item;
import star.active_item;
import star.force_regions;
import star.poly;
import star.actor_movement_controller;
import star.client_context;
import star.casting;
import star.codex_item;
import star.entity_rendering;
import star.fireable_item;
import star.game_types;
import star.particle;
import star.line;
import star.player_codexes;
import star.tools;
import star.tool_user;
import star.uuid;
import star.vector;
import star.item_drop;
import star.image_metadata_database;
import star.rect;
import star.world_server;
namespace Star {

void PrintTo(ItemDescriptor const& descriptor, std::ostream* stream) {
  TextBuffer output;
  auto printed = write_text(output, descriptor);
  if (!printed)
    *stream << "<ItemDescriptor rendering failed: " << printed.error().what() << ">";
  else
    *stream << output.view();
}

}

using namespace Star;

namespace {

TEST(GameTimerTest, SignedCountdownAndSingleExpiration) {
  GameTimer zero;
  EXPECT_TRUE(zero.ready());
  EXPECT_TRUE(zero.wrap_tick(0.0f));
  EXPECT_TRUE(zero.wrap_tick(9.0f));
  EXPECT_EQ(zero.remaining_fraction(), 0.0f);

  GameTimer timer(-4.0f);
  EXPECT_EQ(timer.remaining_fraction(), 1.0f);
  EXPECT_FALSE(timer.tick(1.0f));
  EXPECT_EQ(timer.remaining_fraction(), 0.75f);
  timer.invert();
  EXPECT_EQ(timer.remaining, -1.0f);
  EXPECT_FALSE(timer.tick(-0.5f));
  EXPECT_EQ(timer.remaining, -1.5f);
  EXPECT_TRUE(timer.wrap_tick(10.0f));
  EXPECT_EQ(timer.remaining, timer.duration);
  EXPECT_FALSE(timer.wrap_tick(0.0f));
  timer.set_done();
  EXPECT_TRUE(timer.ready());
  timer.reset();
  EXPECT_TRUE(timer.tick(4.0f));
  EXPECT_TRUE(timer.ready());
}

TEST(GameTimerTest, LegacyFloatsAndTruncatedRead) {
  constexpr std::array<char, 8> wire{0x40, 0, 0, 0, 0x3f, 0, 0, 0}; // duration 2, remaining 0.5
  GameTimer timer(-8.0f);
  timer.tick(1.0f);
  for (std::size_t n = 0; n < wire.size(); ++n) {
    SCOPED_TRACE(n);
    auto read = DataStreamBuffer::deserialize(timer, std::span(wire).first(n));
    ASSERT_FALSE(read);
    EXPECT_TRUE(read.error().is<EofException>());
    EXPECT_EQ(timer.duration, -8.0f);
    EXPECT_EQ(timer.remaining, -7.0f);
  }
  ASSERT_TRUE(DataStreamBuffer::deserialize(timer, wire));
  EXPECT_EQ(timer.duration, 2.0f);
  EXPECT_EQ(timer.remaining, 0.5f);
  DataStreamBuffer encoded;
  ASSERT_TRUE(encoded.write(timer));
  EXPECT_TRUE(std::ranges::equal(encoded.span(), wire));
}

TEST(EpochTimerTest, LegacySaveAndBackwardClockRebase) {
  Json cold_save = JsonObject{{"lastEpochTime", Json()}, {"elapsedTime", 0.0}};
  EXPECT_EQ(EpochTimer{}.to_json(), cold_save);
  auto timer = EpochTimer::from_json(cold_save);
  ASSERT_TRUE(timer);
  timer->update(100);
  timer->update(90);
  EXPECT_EQ(timer->elapsed_time(), 0);
  timer->update(95);
  EXPECT_EQ(timer->elapsed_time(), 5);
  timer->set_elapsed_time(-2);
  timer->update(96);
  Json warm_save = JsonObject{{"lastEpochTime", 96.0}, {"elapsedTime", -1.0}};
  EXPECT_EQ(timer->to_json(), warm_save);
  auto resumed = EpochTimer::from_json(warm_save);
  ASSERT_TRUE(resumed);
  resumed->update(100);
  EXPECT_EQ(resumed->elapsed_time(), 3);
  EXPECT_FALSE(EpochTimer::from_json(JsonObject{{"elapsedTime", 1}}));
  EXPECT_FALSE(EpochTimer::from_json(JsonObject{{"lastEpochTime", Json()}}));
  EXPECT_FALSE(EpochTimer::from_json(JsonObject{{"lastEpochTime", "bad"}, {"elapsedTime", 1}}));
}

TEST(EpochTimerTest, LegacyOptionalAndTruncatedRead) {
  // An optional epoch (absent or 2.0) followed by 3.0 elapsed, in network order.
  constexpr std::array<char, 9> absent{0, 0x40, 8, 0, 0, 0, 0, 0, 0};
  constexpr std::array<char, 17> present{1, 0x40, 0, 0, 0, 0, 0, 0, 0, 0x40, 8, 0, 0, 0, 0, 0, 0};
  for (std::span<char const> bytes : {std::span<char const>(absent), std::span<char const>(present)}) {
    EpochTimer timer;
    timer.update(90);
    timer.set_elapsed_time(-2);
    auto previous = timer.to_json();
    for (std::size_t n = 0; n < bytes.size(); ++n) {
      SCOPED_TRACE(n);
      auto read = DataStreamBuffer::deserialize(timer, bytes.first(n));
      ASSERT_FALSE(read);
      EXPECT_TRUE(read.error().is<EofException>());
      EXPECT_EQ(timer.to_json(), previous);
    }
    ASSERT_TRUE(DataStreamBuffer::deserialize(timer, bytes));
    EXPECT_EQ(timer.elapsed_time(), 3);
    DataStreamBuffer encoded;
    ASSERT_TRUE(encoded.write(timer));
    EXPECT_TRUE(std::ranges::equal(encoded.span(), bytes));
    timer.update(5);
    EXPECT_EQ(timer.elapsed_time(), bytes[0] ? 6 : 3);
  }
}

[[nodiscard]] std::expected<std::shared_ptr<Item>, StarException> inventoryTestItem(
    ItemDatabase const& database, String const& name, std::uint64_t count = 1, Json parameters = Json()) {
  auto descriptor = ItemDescriptor::create(name, count, std::move(parameters));
  if (!descriptor)
    return std::unexpected(std::move(descriptor.error()));
  return database.item(*descriptor);
}

class MutableItem final : public Item {
public:
  explicit MutableItem(Root& root) : Item(root) {}

  using Item::initialize;
  using Item::set_icon_drawables;
  using Item::set_instance_value;
  using Item::set_secondary_icon_drawables;

  std::expected<std::shared_ptr<Item>, StarException> clone() const override {
    return std::make_shared<MutableItem>(*this);
  }
};

}

const GameTestRegistration itemDescriptorConstruction("ItemTest", "ItemDescriptorConstruction", [](GameTest&) {
  struct ConstructionCase {
    char const* label;
    std::expected<ItemDescriptor, StarException> descriptor;
    String name;
    std::uint64_t count;
    Json parameters;
  };
  Json const emptyParameters = JsonObject();
  Json const customParameters = JsonObject{{"testParameter", "testValue"}};
  ConstructionCase const cases[]{
    {"default construction", ItemDescriptor(), "", 0, emptyParameters},
    {"null JSON", ItemDescriptor::create(Json()), "", 0, emptyParameters},
    {"name-only JSON", ItemDescriptor::create(Json("perfectlygenericitem")), "perfectlygenericitem", 1,
     emptyParameters},
    {"array: omitted count and parameters", ItemDescriptor::create(JsonArray{"perfectlygenericitem"}),
     "perfectlygenericitem", 1, emptyParameters},
    {"array: omitted parameters", ItemDescriptor::create(JsonArray{"perfectlygenericitem", 3}), "perfectlygenericitem",
     3, emptyParameters},
    {"array: empty parameters", ItemDescriptor::create(JsonArray{"perfectlygenericitem", 3, emptyParameters}),
     "perfectlygenericitem", 3, emptyParameters},
    {"array: custom parameters", ItemDescriptor::create(JsonArray{"perfectlygenericitem", 3, customParameters}),
     "perfectlygenericitem", 3, customParameters},
    {"object name: omitted count and parameters", ItemDescriptor::create(JsonObject{{"name", "perfectlygenericitem"}}),
     "perfectlygenericitem", 1, emptyParameters},
    {"object item: omitted count and parameters", ItemDescriptor::create(JsonObject{{"item", "perfectlygenericitem"}}),
     "perfectlygenericitem", 1, emptyParameters},
    {"object name: omitted parameters",
     ItemDescriptor::create(JsonObject{{"name", "perfectlygenericitem"}, {"count", 3}}), "perfectlygenericitem", 3,
     emptyParameters},
    {"object name: empty parameters",
     ItemDescriptor::create(
       JsonObject{{"name", "perfectlygenericitem"}, {"count", 3}, {"parameters", emptyParameters}}),
     "perfectlygenericitem", 3, emptyParameters},
    {"object name: custom parameters",
     ItemDescriptor::create(
       JsonObject{{"name", "perfectlygenericitem"}, {"count", 3}, {"parameters", customParameters}}),
     "perfectlygenericitem", 3, customParameters},
    {"direct: omitted parameters", ItemDescriptor::create("perfectlygenericitem", 3), "perfectlygenericitem", 3,
     emptyParameters},
    {"direct: empty parameters", ItemDescriptor::create("perfectlygenericitem", 3, emptyParameters),
     "perfectlygenericitem", 3, emptyParameters},
    {"direct: custom parameters", ItemDescriptor::create("perfectlygenericitem", 3, customParameters),
     "perfectlygenericitem", 3, customParameters}};
  for (auto const& test : cases) {
    SCOPED_TRACE(test.label);
    ASSERT_TRUE(test.descriptor);
    EXPECT_EQ(test.descriptor->name(), test.name);
    EXPECT_EQ(test.descriptor->count(), test.count);
    EXPECT_EQ(test.descriptor->parameters(), test.parameters);
  }
});

const GameTestRegistration itemFallbackDiagnostics(
    "ItemTest", "FallbackKeepsDescriptorWhileLogsSeparateSummaryAndSavedState", [](GameTest& fixture) {
      class DiagnosticSink final : public LogSink {
      public:
        std::vector<std::pair<LogLevel, std::string>> records;
        std::expected<void, StarException> log(std::string_view text, LogLevel level) override {
          records.emplace_back(level, text);
          return {};
        }
      };
      auto directory = File::temporary_directory(fixture.root().random());
      ASSERT_TRUE(directory);
      auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
      auto settings = fixture.rootSettings();
      settings.storageDirectory = File::relative_to(*directory, "storage");
      settings.runtimeConfigFile = {};
      settings.logDirectory = {};
      settings.logFile = {};
      settings.logLevel = LogLevel::Error;
      settings.includeUGC = false;
      auto root = Root::create(std::move(settings));
      ASSERT_TRUE(root);
      auto database = (*root)->itemDatabase();
      ASSERT_TRUE(database);
      auto sink = std::make_shared<DiagnosticSink>();
      sink->set_level(LogLevel::Error);
      (*root)->logger().add_sink(sink);
      auto descriptor = ItemDescriptor::create("missing-item-diagnostics-probe", 37,
          JsonObject{{"scriptStorage", JsonObject{{"savedState", "private-saved-state-marker"}}}});
      ASSERT_TRUE(descriptor);
      auto storedDescriptor = descriptor->toJson().erase("count");
      ASSERT_TRUE(storedDescriptor);
      for (auto level : {LogLevel::Error, LogLevel::Debug}) {
        sink->set_level(level);
        sink->records.clear();
        auto item = (*database)->item(*descriptor);
        ASSERT_TRUE(item);
        ASSERT_TRUE(*item);
        EXPECT_EQ((*item)->name(), "perfectlygenericitem");
        EXPECT_EQ((*item)->count(), 37u);
        EXPECT_EQ((*item)->descriptor().parameters().get("genericItemStorage"), *storedDescriptor);
        bool sawError = false;
        bool sawSavedState = false;
        for (auto const& [recordLevel, text] : sink->records) {
          if (recordLevel == LogLevel::Error) {
            sawError = true;
            EXPECT_NE(text.find("missing-item-diagnostics-probe"), std::string::npos);
            EXPECT_NE(text.find("37"), std::string::npos);
            EXPECT_EQ(text.find("private-saved-state-marker"), std::string::npos);
            EXPECT_EQ(text.find("Caused by:"), std::string::npos);
            EXPECT_EQ(text.find("Details:"), std::string::npos);
          }
          if (text.find("private-saved-state-marker") != std::string::npos) {
            sawSavedState = true;
            EXPECT_EQ(recordLevel, LogLevel::Debug);
          }
        }
        EXPECT_TRUE(sawError);
        EXPECT_EQ(sawSavedState, level == LogLevel::Debug);
      }
    });

const GameTestRegistration itemComparison("ItemTest", "ItemComparison", [](GameTest& fixture) {
  auto database = fixture.root().itemDatabase();
  ASSERT_TRUE(database);
  auto& itemDatabase = **database;
  auto plainItem = inventoryTestItem(itemDatabase, "perfectlygenericitem");
  ASSERT_TRUE(plainItem);
  auto testItem = std::move(*plainItem);
  ASSERT_TRUE(testItem);
  auto parameterizedItem = inventoryTestItem(itemDatabase, "perfectlygenericitem", 1, JsonObject{{"testParameter", "testValue"}});
  ASSERT_TRUE(parameterizedItem);
  auto testItemParams = std::move(*parameterizedItem);
  ASSERT_TRUE(testItemParams);

  auto plainDescriptor = ItemDescriptor::create("perfectlygenericitem", 1);
  ASSERT_TRUE(plainDescriptor);
  auto parameterizedDescriptor =
    ItemDescriptor::create("perfectlygenericitem", 1, JsonObject{{"testParameter", "testValue"}});
  ASSERT_TRUE(parameterizedDescriptor);

  // Creating an item can add parameters to it: the perfectly generic item is an
  // object item, and objects that retain their placement parameters start with
  // an empty 'scriptStorage' entry so that they stack correctly.  Exact
  // comparisons therefore work on the parameters the created items carry.
  ItemDescriptor testItemExact = testItem->descriptor().singular();
  ItemDescriptor testItemParamsExact = testItemParams->descriptor().singular();
  EXPECT_EQ(testItemExact.parameters(), JsonObject({{"scriptStorage", JsonObject()}}));
  EXPECT_EQ(testItemParamsExact.parameters(),
      JsonObject({{"testParameter", "testValue"}, {"scriptStorage", JsonObject()}}));

  // Parsing formats are checked independently above; matching needs only one
  // representative of each distinct parameter set.
  enum class ParameterClass { Empty, Custom, Storage, CustomStorage };
  struct DescriptorCase {
    char const* label;
    ItemDescriptor descriptor;
    ParameterClass parameters;
  };
  struct ItemCase {
    char const* label;
    Item const* item;
    ParameterClass parameters;
  };
  DescriptorCase const descriptors[]{
    {"empty parameters", std::move(*plainDescriptor), ParameterClass::Empty},
    {"custom parameters", std::move(*parameterizedDescriptor), ParameterClass::Custom},
    {"scriptStorage", std::move(testItemExact), ParameterClass::Storage},
    {"custom parameters and scriptStorage", std::move(testItemParamsExact), ParameterClass::CustomStorage}};
  ItemCase const items[]{{"plain item with scriptStorage", testItem.get(), ParameterClass::Storage},
                         {"custom item with scriptStorage", testItemParams.get(), ParameterClass::CustomStorage}};

  for (bool exactMatch : {false, true}) {
    SCOPED_TRACE(exactMatch ? "exact matching" : "non-exact matching");
    for (auto const& left : descriptors) {
      SCOPED_TRACE(left.label);
      for (auto const& right : descriptors) {
        SCOPED_TRACE(right.label);
        EXPECT_EQ(left.descriptor.matches(right.descriptor, exactMatch),
                  !exactMatch || left.parameters == right.parameters);
      }
      for (auto const& item : items) {
        SCOPED_TRACE(item.label);
        bool expectedMatch = !exactMatch || left.parameters == item.parameters;
        EXPECT_EQ(left.descriptor.matches(*item.item, exactMatch), expectedMatch);
        EXPECT_EQ(item.item->matches(left.descriptor, exactMatch), expectedMatch);
      }
    }
    for (auto const& left : items) {
      SCOPED_TRACE(left.label);
      for (auto const& right : items) {
        SCOPED_TRACE(right.label);
        EXPECT_EQ(left.item->matches(*right.item, exactMatch), !exactMatch || left.parameters == right.parameters);
      }
    }
  }
});

const GameTestRegistration itemStackingPreservesCountsAndNullSemantics("ItemTest", "ItemStackingPreservesCountsAndNullSemantics", [](GameTest& fixture) {
  auto database = fixture.root().itemDatabase();
  ASSERT_TRUE(database);
  auto targetResult = inventoryTestItem(**database, "perfectlygenericitem", 4, JsonObject{{"maxStack", 5}});
  auto sourceResult = inventoryTestItem(**database, "perfectlygenericitem", 3, JsonObject{{"maxStack", 5}});
  auto differentResult = inventoryTestItem(**database, "perfectlygenericitem", 3,
      JsonObject{{"maxStack", 5}, {"testParameter", "different"}});
  ASSERT_TRUE(targetResult);
  ASSERT_TRUE(sourceResult);
  ASSERT_TRUE(differentResult);
  auto target = std::move(*targetResult);
  auto source = std::move(*sourceResult);
  auto different = std::move(*differentResult);
  ASSERT_TRUE(target);
  ASSERT_TRUE(source);
  ASSERT_TRUE(different);
  ASSERT_EQ(target->max_stack(), 5u);

  EXPECT_FALSE(target->stackable_with(std::nullopt));
  EXPECT_EQ(target->could_stack(std::nullopt), 0u);
  EXPECT_FALSE(target->stack_with(std::nullopt));
  EXPECT_TRUE(Item::items_equal(std::nullopt, std::nullopt));
  EXPECT_FALSE(Item::items_equal(std::cref(*target), std::nullopt));
  EXPECT_FALSE(Item::items_equal(std::nullopt, std::cref(*target)));
  EXPECT_TRUE(Item::items_equal(std::cref(*target), std::cref(*source)));
  EXPECT_TRUE(target->matches(*different));
  EXPECT_FALSE(target->matches(*different, true));
  EXPECT_FALSE(target->stackable_with(std::cref(*different)));
  EXPECT_EQ(target->could_stack(std::cref(*different)), 0u);
  EXPECT_FALSE(target->stack_with(std::ref(*different)));
  EXPECT_EQ(different->count(), 3u);
  EXPECT_EQ(target->count(), 4u);

  EXPECT_EQ(target->could_stack(std::cref(*source)), 1u);
  EXPECT_TRUE(target->stack_with(std::ref(*source)));
  EXPECT_EQ(target->count(), 5u);
  EXPECT_EQ(source->count(), 2u);
  EXPECT_EQ(target->could_stack(std::cref(*source)), 0u);
  EXPECT_FALSE(target->stack_with(std::ref(*source)));
  EXPECT_EQ(source->count(), 2u);

  ASSERT_TRUE(target->consume(3));
  EXPECT_EQ(target->could_stack(std::cref(*source)), 2u);
  EXPECT_TRUE(target->stack_with(std::ref(*source)));
  EXPECT_EQ(target->count(), 4u);
  EXPECT_TRUE(source->empty());
  EXPECT_FALSE(target->stack_with(std::ref(*source)));
  EXPECT_EQ(target->count(), 4u);

  // The source may alias the destination: consuming and adding must balance.
  target->set_count(2);
  EXPECT_TRUE(target->stack_with(std::ref(*target)));
  EXPECT_EQ(target->count(), 2u);
});

const GameTestRegistration item_take_overfill("ItemTest", "TakingOverfilledItemConservesCount", [](GameTest& fixture) {
  auto source = GenericItem::create(fixture.root(),
      JsonObject{{"itemName", "takeprobe"}, {"rarity", "common"}, {"maxStack", 5}}, "/", JsonObject{{"marker", 17}});
  ASSERT_TRUE(source) << source.error().what();
  ASSERT_EQ((*source)->set_count(12, true), 0u);
  auto const original = (*source)->descriptor();

  auto zero = (*source)->take(0);
  ASSERT_TRUE(zero) << zero.error().what();
  EXPECT_FALSE(*zero);
  EXPECT_EQ((*source)->descriptor(), original);

  auto first = (*source)->take(8);
  ASSERT_TRUE(first) << first.error().what();
  ASSERT_TRUE(*first);
  EXPECT_EQ((*first)->count(), 5u);
  EXPECT_EQ((*source)->count(), 7u);
  EXPECT_TRUE((*first)->matches(original, true));
  EXPECT_TRUE((*source)->matches(original, true));

  auto second = (*source)->take();
  ASSERT_TRUE(second) << second.error().what();
  ASSERT_TRUE(*second);
  EXPECT_EQ((*second)->count(), 5u);
  EXPECT_EQ((*source)->count(), 2u);

  auto last = (*source)->take();
  ASSERT_TRUE(last) << last.error().what();
  ASSERT_TRUE(*last);
  EXPECT_EQ((*last)->count(), 2u);
  EXPECT_TRUE((*source)->empty());
  EXPECT_EQ((*first)->count() + (*second)->count() + (*last)->count(), original.count());
});

const GameTestRegistration item_drop_take_overfill("ItemTest", "OverfilledDropStaysTakeableUntilDrained", [](GameTest& fixture) {
  for (bool take_by : {false, true}) {
    SCOPED_TRACE(take_by);
    auto item = GenericItem::create(fixture.root(),
        JsonObject{{"itemName", "dropprobe"}, {"rarity", "common"}, {"maxStack", 5}}, "/", JsonObject{});
    ASSERT_TRUE(item) << item.error().what();
    auto drop = ItemDrop::create(fixture.root(), std::move(*item));
    ASSERT_TRUE(drop) << drop.error().what();
    auto source = (*drop)->item();
    ASSERT_EQ(source->set_count(12, true), 0u);

    auto take = [&] { return take_by ? (*drop)->takeBy(101) : (*drop)->take(); };
    std::uint64_t remaining = source->count();
    for (std::uint64_t expected : {5u, 5u, 2u}) {
      EXPECT_EQ((*drop)->canTake(), true);
      auto picked_up = take();
      ASSERT_TRUE(picked_up) << picked_up.error().what();
      ASSERT_TRUE(*picked_up);
      EXPECT_EQ((*picked_up)->count(), expected);
      remaining -= expected;
      EXPECT_EQ(source->count(), remaining);
      EXPECT_EQ((*drop)->persistent(), !take_by || remaining != 0);
    }
    EXPECT_EQ((*drop)->canTake(), false);
    auto empty = take();
    ASSERT_TRUE(empty) << empty.error().what();
    EXPECT_FALSE(*empty);
  }
});

const GameTestRegistration item_base_tags("ItemTest", "TagsReadBaseConfigAndValidateTheEntireArray", [](GameTest& fixture) {
  Json const config = JsonObject{{"itemName", "tagprobe"}, {"rarity", "common"}};
  Json const parameters = JsonObject{{"itemTags", JsonArray{"instance"}}};
  auto valid_config = config.set("itemTags", JsonArray{"base", "base", "other"});
  ASSERT_TRUE(valid_config) << valid_config.error().what();
  auto valid = GenericItem::create(fixture.root(), *valid_config, "/", parameters);
  ASSERT_TRUE(valid) << valid.error().what();
  EXPECT_EQ((*valid)->has_item_tag("base"), true);
  EXPECT_EQ((*valid)->has_item_tag("other"), true);
  EXPECT_EQ((*valid)->has_item_tag("instance"), false);
  auto tags = (*valid)->item_tags();
  ASSERT_TRUE(tags) << tags.error().what();
  EXPECT_EQ(tags->size(), 2u);
  EXPECT_TRUE(tags->contains("base"));
  EXPECT_TRUE(tags->contains("other"));

  auto absent = GenericItem::create(fixture.root(), config, "/", parameters);
  ASSERT_TRUE(absent) << absent.error().what();
  EXPECT_EQ((*absent)->has_item_tag("instance"), false);

  auto malformed_config = config.set("itemTags", JsonArray{"base", Json(17)});
  ASSERT_TRUE(malformed_config) << malformed_config.error().what();
  auto malformed = GenericItem::create(fixture.root(), *malformed_config, "/", parameters);
  ASSERT_TRUE(malformed) << malformed.error().what();
  auto has_tag = (*malformed)->has_item_tag("base");
  auto malformed_tags = (*malformed)->item_tags();
  ASSERT_FALSE(has_tag);
  ASSERT_FALSE(malformed_tags);
  EXPECT_TRUE(has_tag.error().is<JsonException>());
  EXPECT_TRUE(malformed_tags.error().is<JsonException>());
});

const GameTestRegistration item_parameter_updates("ItemTest", "InstanceValueUpdatesPreserveMissingNullAndObjectMerge", [](GameTest& fixture) {
  MutableItem item(fixture.root());
  Json const base = JsonObject{{"baseOnly", 1}, {"shared", 2}};
  Json const instance = JsonObject{{"shared", 3}, {"instanceOnly", 4}};
  Json const merged = JsonObject{{"baseOnly", 1}, {"shared", 3}, {"instanceOnly", 4}};
  Json const config = JsonObject{{"itemName", "parameterprobe"}, {"rarity", "common"}, {"probe", base}};
  ASSERT_TRUE(item.initialize(config, "/"));

  auto const initial_storage = item.parameters().object_ptr();
  ASSERT_TRUE(initial_storage);
  ASSERT_TRUE(item.set_instance_value("probe", Json()));
  EXPECT_EQ(item.parameters().object_ptr(), *initial_storage);
  EXPECT_EQ(item.parameters().contains("probe"), false);
  EXPECT_EQ(item.instance_value("probe"), base);

  ASSERT_TRUE(item.set_instance_value("probe", instance));
  auto const saved = item.descriptor();
  EXPECT_EQ(saved.parameters().get("probe"), instance);
  EXPECT_EQ(item.instance_value("probe"), merged);
  auto const saved_storage = saved.parameters().object_ptr();
  ASSERT_TRUE(saved_storage);
  ASSERT_TRUE(item.set_instance_value("probe", instance));
  EXPECT_EQ(item.parameters().object_ptr(), *saved_storage);
  EXPECT_EQ(item.descriptor(), saved);

  ASSERT_TRUE(item.set_instance_value("probe", Json()));
  EXPECT_EQ(item.parameters().contains("probe"), true);
  EXPECT_EQ(item.parameters().get("probe"), Json());
  EXPECT_EQ(item.instance_value("probe"), base);
  EXPECT_EQ(item.config(), config);
});

const GameTestRegistration item_icon_normalization("ItemTest", "AuthoredAndUpdatedIconsShareNormalization", [](GameTest& fixture) {
  auto metadata = fixture.root().image_metadata_database();
  ASSERT_TRUE(metadata) << metadata.error().what();
  Json const layer = JsonObject{{"poly", JsonArray{JsonArray{0, 0}, JsonArray{32, 0}, JsonArray{0, 24}}},
      {"position", JsonArray{9, -13}}};
  Json const config = JsonObject{{"itemName", "iconprobe"}, {"rarity", "common"},
      {"inventoryIcon", JsonArray{layer}}, {"secondaryIcon", JsonArray{layer}}};
  MutableItem item(fixture.root());
  ASSERT_TRUE(item.initialize(config, "/"));
  auto authored = item.icon_drawables();
  ASSERT_TRUE(authored);
  ASSERT_EQ(authored->size(), 1u);
  auto secondary = item.secondary_drawables();
  ASSERT_TRUE(secondary);
  ASSERT_TRUE(*secondary);
  EXPECT_EQ(**secondary, *authored);
  auto bounds = Drawable::bound_box_all(**metadata, *authored, true);
  ASSERT_TRUE(bounds) << bounds.error().what();
  EXPECT_NEAR(bounds->center()[0], 0.0f, 0.0001f);
  EXPECT_NEAR(bounds->center()[1], 0.0f, 0.0001f);
  EXPECT_NEAR(std::max(bounds->width(), bounds->height()), 16.0f, 0.0001f);

  auto raw = Drawable::create(**metadata, layer);
  ASSERT_TRUE(raw) << raw.error().what();
  ASSERT_TRUE(item.set_icon_drawables({*raw}));
  EXPECT_EQ(item.icon_drawables(), *authored);
  ASSERT_TRUE(item.set_secondary_icon_drawables(List<Drawable>{*raw}));
  auto updated_secondary = item.secondary_drawables();
  ASSERT_TRUE(updated_secondary);
  ASSERT_TRUE(*updated_secondary);
  EXPECT_EQ(**updated_secondary, *authored);
  ASSERT_TRUE(item.set_secondary_icon_drawables(List<Drawable>{}));
  EXPECT_TRUE(item.has_secondary_drawables());
  secondary = item.secondary_drawables();
  ASSERT_TRUE(secondary);
  ASSERT_TRUE(*secondary);
  EXPECT_TRUE((*secondary)->empty());
  ASSERT_TRUE(item.set_secondary_icon_drawables(std::nullopt));
  EXPECT_FALSE(item.has_secondary_drawables());
});

const GameTestRegistration bagStackQueriesAndTransfersPreserveBehavior("ItemTest", "BagStackQueriesAndTransfersPreserveBehavior", [](GameTest& fixture) {
  auto database = fixture.root().itemDatabase();
  ASSERT_TRUE(database);
  auto targetResult = inventoryTestItem(**database, "perfectlygenericitem", 4, JsonObject{{"maxStack", 5}});
  auto blockerResult = inventoryTestItem(**database, "perfectlygenericitem", 5,
      JsonObject{{"maxStack", 5}, {"testParameter", "different"}});
  auto sourceResult = inventoryTestItem(**database, "perfectlygenericitem", 3, JsonObject{{"maxStack", 5}});
  ASSERT_TRUE(targetResult);
  ASSERT_TRUE(blockerResult);
  ASSERT_TRUE(sourceResult);
  auto target = std::move(*targetResult);
  auto blocker = std::move(*blockerResult);
  auto source = std::move(*sourceResult);
  ASSERT_TRUE(target);
  ASSERT_TRUE(blocker);
  ASSERT_TRUE(source);
  ItemBag bag(fixture.root(), 2);
  ASSERT_TRUE(bag.setItem(0, target));
  ASSERT_TRUE(bag.setItem(1, blocker));

  EXPECT_EQ(bag.itemsCanFit(std::nullopt), 0u);
  EXPECT_EQ(bag.itemsCanStack(std::nullopt), 0u);
  auto nullFit = bag.itemsFitWhere(std::nullopt);
  EXPECT_EQ(nullFit.leftover, 0u);
  EXPECT_TRUE(nullFit.slots.empty());
  auto fit = bag.itemsFitWhere(std::cref(*source));
  EXPECT_EQ(fit.leftover, 2u);
  EXPECT_EQ(fit.slots, List<std::size_t>({0}));
  EXPECT_EQ(bag.itemsCanFit(std::cref(*source)), 1u);
  EXPECT_EQ(bag.itemsCanStack(std::cref(*source)), 1u);
  auto limitedFit = bag.itemsFitWhere(std::cref(*source), 2);
  EXPECT_EQ(limitedFit.leftover, 1u);
  EXPECT_EQ(limitedFit.slots, List<std::size_t>({0}));
  auto zeroFit = bag.itemsFitWhere(std::cref(*source), 0);
  EXPECT_EQ(zeroFit.leftover, 0u);
  EXPECT_TRUE(zeroFit.slots.empty());
  EXPECT_EQ(target->count(), 4u);
  EXPECT_EQ(source->count(), 3u);
  EXPECT_EQ(blocker->count(), 5u);

  auto stacked = bag.stackItems(source);
  ASSERT_TRUE(stacked);
  EXPECT_EQ(*stacked, source);
  EXPECT_EQ(source->count(), 2u);
  EXPECT_EQ(target->count(), 5u);
  auto put = bag.putItems(0, source);
  ASSERT_TRUE(put);
  EXPECT_EQ(*put, source);
  EXPECT_EQ(source->count(), 2u);
  auto swapped = bag.swapItems(0, source);
  ASSERT_TRUE(swapped);
  EXPECT_EQ(*swapped, target);
  auto current = std::as_const(bag).at(0);
  ASSERT_TRUE(current);
  EXPECT_EQ(*current, source);
  auto removed = bag.swapItems(0, {});
  ASSERT_TRUE(removed);
  EXPECT_EQ(*removed, source);
  auto added = bag.addItems(target);
  ASSERT_TRUE(added);
  EXPECT_FALSE(*added);
  current = std::as_const(bag).at(0);
  ASSERT_TRUE(current);
  EXPECT_EQ(*current, target);

  // An externally exhausted stack is an empty slot, without query-time release.
  ASSERT_TRUE(blocker->consume(5));
  fit = bag.itemsFitWhere(std::cref(*source));
  EXPECT_EQ(fit.leftover, 0u);
  EXPECT_EQ(fit.slots, List<std::size_t>({1}));
  EXPECT_EQ(bag.itemsCanFit(std::cref(*source)), 2u);
  // A retained empty stack contributes capacity to the existing stacking query.
  EXPECT_EQ(bag.itemsCanStack(std::cref(*source)), 2u);
  blocker->set_count(1);
  current = std::as_const(bag).at(1);
  ASSERT_TRUE(current);
  EXPECT_EQ(*current, blocker);
  blocker->set_count(0);
  added = bag.addItems(source);
  ASSERT_TRUE(added);
  EXPECT_FALSE(*added);
  current = std::as_const(bag).at(1);
  ASSERT_TRUE(current);
  EXPECT_EQ(*current, source);
  source->set_count(0);
  EXPECT_EQ(bag.itemsCanFit(std::cref(*source)), 0u);
  EXPECT_EQ(bag.itemsCanStack(std::cref(*source)), 0u);
  auto emptyFit = bag.itemsFitWhere(std::cref(*source));
  EXPECT_EQ(emptyFit.leftover, 0u);
  EXPECT_TRUE(emptyFit.slots.empty());
});

const GameTestRegistration constructItems("ItemTest", "ConstructItems", [](GameTest& fixture) {
  auto itemDatabase = fixture.root().itemDatabase();
  ASSERT_TRUE(itemDatabase);
  for (auto const& itemName : (*itemDatabase)->allItems()) {
    auto item = inventoryTestItem(**itemDatabase, itemName);
    ASSERT_TRUE(item);
  }
});

const GameTestRegistration constBagObservesEmptyStacksWithoutReleasingThem("ItemTest", "ConstBagObservesEmptyStacksWithoutReleasingThem", [](GameTest& fixture) {
  auto itemDatabase = fixture.root().itemDatabase();
  ASSERT_TRUE(itemDatabase);
  auto makeEmptyBag = [&]() -> std::expected<ItemBag, StarException> {
    ItemBag bag(fixture.root(), 2);
    for (std::size_t slot = 0; slot != 2; ++slot) {
      auto item = inventoryTestItem(**itemDatabase, "perfectlygenericitem");
      if (!item)
        return std::unexpected(std::move(item.error()));
      auto set = bag.setItem(slot, std::move(*item));
      if (!set)
        return std::unexpected(std::move(set.error()));
    }
    return bag;
  };

  auto indexedResult = makeEmptyBag();
  ASSERT_TRUE(indexedResult);
  ItemBag const indexedBag = std::move(*indexedResult);
  auto firstResult = indexedBag.at(0);
  ASSERT_TRUE(firstResult);
  auto first = std::move(*firstResult);
  ASSERT_TRUE(first);
  auto secondResult = indexedBag.at(1);
  ASSERT_TRUE(secondResult);
  auto second = std::move(*secondResult);
  ASSERT_TRUE(second);
  first->set_count(0);
  second->set_count(0);
  auto firstOwners = first.use_count();
  EXPECT_EQ(indexedBag.at(0), nullptr);
  EXPECT_EQ(first.use_count(), firstOwners);
  first->set_count(1);
  EXPECT_EQ(indexedBag.at(0), first);
  first->set_count(0);
  EXPECT_EQ(indexedBag.size(), 2u);
  EXPECT_TRUE(indexedBag.cleanup());
  EXPECT_EQ(indexedBag.at(1), nullptr);
  EXPECT_FALSE(indexedBag.cleanup());

  auto listedResult = makeEmptyBag();
  ASSERT_TRUE(listedResult);
  ItemBag const listedBag = std::move(*listedResult);
  auto emptyResult = listedBag.at(0);
  ASSERT_TRUE(emptyResult);
  auto emptyItem = std::move(*emptyResult);
  ASSERT_TRUE(emptyItem);
  auto liveResult = listedBag.at(1);
  ASSERT_TRUE(liveResult);
  auto liveItem = std::move(*liveResult);
  ASSERT_TRUE(liveItem);
  emptyItem->set_count(0);
  auto const& slots = listedBag.items();
  ASSERT_EQ(slots.size(), 2u);
  EXPECT_FALSE(slots[0]);
  EXPECT_EQ(slots[1], liveItem);
  EXPECT_FALSE(listedBag.cleanup());
});

namespace {

[[nodiscard]] std::expected<std::shared_ptr<Item>, StarException> inventoryTestHeadArmor(Root& root) {
  auto itemDatabase = root.itemDatabase();
  if (!itemDatabase)
    return std::unexpected(std::move(itemDatabase.error()));
  for (auto const& name : (*itemDatabase)->allItems()) {
    auto type = (*itemDatabase)->itemType(name);
    if (!type)
      return std::unexpected(std::move(type.error()));
    if (*type == ItemType::HeadArmor)
      return inventoryTestItem(**itemDatabase, name);
  }
  return std::unexpected(ItemException("Inventory tests require a head armor item"));
}

[[nodiscard]] std::expected<BagSlot, StarException> inventoryTestBagSlot(
    Root& root, PlayerInventory const& inventory, std::shared_ptr<Item> const& item) {
  // The perfectly generic item is an ObjectItem, not a main-bag generic item.
  // Use the real configured filters rather than forcing it into a fixed bag.
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/player.config:inventory");
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto bags = config->get("itemBags");
  if (!bags)
    return std::unexpected(std::move(bags.error()));
  auto object = bags->to_object();
  if (!object)
    return std::unexpected(std::move(object.error()));
  for (auto const& name : object->keys()) {
    auto bag = inventory.bagContents(name);
    if (bag && bag->size() != 0) {
      auto allowed = inventory.itemAllowedInBag(item, name);
      if (!allowed)
        return std::unexpected(std::move(allowed.error()));
      if (*allowed)
        return BagSlot(name, 0);
    }
  }
  return std::unexpected(ItemException("Inventory tests require a bag accepting the fixture item"));
}

}

const GameTestRegistration crafting_batch_accounting("ItemTest", "CraftingBatchAccounting", [](GameTest& fixture) {
  auto& root = fixture.root();
  auto directory = File::temporary_directory(root.random());
  ASSERT_TRUE(directory);
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto statistics = Statistics::create(root, *directory);
  ASSERT_TRUE(statistics);
  auto db = root.itemDatabase();
  ASSERT_TRUE(db);
  auto factory = root.playerFactory();
  ASSERT_TRUE(factory);
  auto made = (*factory)->create();
  ASSERT_TRUE(made);
  auto& player = **made;
  player.setStatistics(*statistics);
  ASSERT_TRUE(player.finalizeCreation());
  auto storage = File::ephemeral_file(root.random());
  ASSERT_TRUE(storage);
  auto world = WorldServer::create(root, Vec2U(2048, 2048), *storage);
  ASSERT_TRUE(world);
  auto close = make_scope_exit([&] { EXPECT_TRUE((*world)->close()); });
  auto uuid = player.uuid();
  ASSERT_TRUE(uuid);
  auto context = ClientContext::create(root.logger(), Uuid(root.random()), *uuid, 1, {});
  ASSERT_TRUE(context);
  player.set_client_context(*context);
  Entity& entity = player;
  ASSERT_TRUE(entity.init(**world, 100, EntityMode::Master));
  auto uninit = make_scope_exit([&] { EXPECT_TRUE(entity.uninit()); });
  ASSERT_FALSE(player.isAdmin());
  auto& inventory = *player.inventory();
  auto plain = inventoryTestItem(**db, "perfectlygenericitem", 4);
  auto marked = inventoryTestItem(**db, "perfectlygenericitem", 7, JsonObject{{"grade", 1}});
  auto result = ItemDescriptor::create("perfectlygenericitem", 3, JsonObject{{"product", true}, {"maxStack", 2}});
  ASSERT_TRUE(plain && marked && result);
  auto product = (*db)->item(result->singular());
  ASSERT_TRUE(product && *product);
  auto product_id = (*product)->descriptor();
  auto slot = inventoryTestBagSlot(root, inventory, *marked);
  ASSERT_TRUE(slot);
  ASSERT_EQ(inventory.setItem(*slot, *marked), true);
  ASSERT_EQ(inventory.setItem(SwapSlot{}, *plain), true);
  auto plain_id = (*plain)->descriptor().singular();
  auto marked_id = (*marked)->descriptor().singular();
  auto zero = inventory.takeItems(marked_id.withCount(0), false, true);
  ASSERT_TRUE(zero);
  EXPECT_EQ(zero->count(), 0u);
  EXPECT_EQ(inventory.hasCountOfItem(marked_id.withCount(3), true), 7u);
  ItemRecipe recipe{};
  recipe.output = *result;
  recipe.inputs = {plain_id, plain_id};
  recipe.matchInputParameters = true;
  recipe.currencyInputs["money"] = 2;
  auto starting_money = inventory.currency("money");
  ASSERT_TRUE(inventory.consumeCurrency("money", starting_money));
  ASSERT_TRUE(inventory.addCurrency("money", 6));
  auto stock = inventory.availableItems();
  ASSERT_TRUE(stock);
  auto limit = [&] { return ItemDatabase::max_craftable_in_bag(*stock, inventory.availableCurrencies(), recipe); };
  auto empty_parameters = ItemDescriptor::create(plain_id.name());
  ASSERT_TRUE(empty_parameters);
  EXPECT_EQ(ItemDatabase::get_count_of_item(*stock, *empty_parameters, true), 0u);
  EXPECT_EQ(ItemDatabase::get_count_of_item(*stock, *empty_parameters, false), 11u);
  EXPECT_EQ(limit(), 2u);
  auto exact_recipe = recipe;
  recipe.matchInputParameters = false;
  EXPECT_NE(exact_recipe, recipe);
  recipe.inputs[1] = marked_id;
  EXPECT_EQ(limit(), 3u);
  recipe.matchInputParameters = true;
  recipe.inputs[1] = plain_id;
  EXPECT_EQ((*db)->craft_recipe(player, recipe, 3), false);
  EXPECT_EQ(inventory.currency("money"), 6u);
  EXPECT_EQ(inventory.hasCountOfItem(plain_id, true), 4u);
  EXPECT_EQ(inventory.hasCountOfItem(product_id, true), 0u);
  recipe.currencyInputs["money"] = 4;
  EXPECT_EQ((*db)->craft_recipe(player, recipe, 2), false);
  EXPECT_EQ(inventory.hasCountOfItem(plain_id, true), 4u);
  recipe.currencyInputs["money"] = 2;
  auto crafted = (*db)->craft_recipe(player, recipe, 2);
  ASSERT_TRUE(crafted) << crafted.error().what();
  ASSERT_TRUE(*crafted);
  EXPECT_EQ(inventory.hasCountOfItem(plain_id, true), 0u);
  EXPECT_EQ(inventory.hasCountOfItem(marked_id, true), 7u);
  EXPECT_EQ(inventory.currency("money"), 2u);
  EXPECT_EQ(inventory.hasCountOfItem(product_id, true), 6u);

  recipe.matchInputParameters = false;
  recipe.inputs = {plain_id, marked_id};
  ASSERT_EQ((*db)->craft_recipe(player, recipe, 1), true);
  EXPECT_EQ(inventory.hasCountOfItem(marked_id, true), 5u);
  EXPECT_EQ(inventory.currency("money"), 0u);
  EXPECT_EQ(inventory.hasCountOfItem(product_id, true), 9u);

  constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
  recipe.matchInputParameters = true;
  recipe.currencyInputs.clear();
  stock->by_parameters[plain_id] = maximum;
  recipe.inputs = {plain_id.withCount(maximum), plain_id};
  EXPECT_EQ(limit(), 0u);
  EXPECT_FALSE(ItemDatabase::can_make_recipe(recipe, *stock, inventory.availableCurrencies()));
  recipe.inputs.clear();
  recipe.currencyInputs.clear();
  EXPECT_EQ(limit(), maximum / 3);
  EXPECT_EQ((*db)->craft_recipe(player, recipe, maximum), false);
});

const GameTestRegistration constInventoryObservesExternallyConsumedItemsWithoutMutation("ItemTest", "ConstInventoryObservesExternallyConsumedItemsWithoutMutation", [](GameTest& fixture) {
  auto itemDatabase = fixture.root().itemDatabase();
  ASSERT_TRUE(itemDatabase);
  auto createdInventory = PlayerInventory::create(fixture.root());
  ASSERT_TRUE(createdInventory);
  auto& inventory = **createdInventory;
  auto const& view = std::as_const(inventory);
  auto bagItemResult = inventoryTestItem(**itemDatabase, "perfectlygenericitem");
  ASSERT_TRUE(bagItemResult);
  auto bagItem = std::move(*bagItemResult);
  auto slotResult = inventoryTestBagSlot(fixture.root(), view, bagItem);
  ASSERT_TRUE(slotResult);
  auto bagSlot = *slotResult;
  auto head = inventoryTestHeadArmor(fixture.root());
  ASSERT_TRUE(head);
  auto swap = inventoryTestItem(**itemDatabase, "perfectlygenericitem");
  ASSERT_TRUE(swap);
  auto trash = inventoryTestItem(**itemDatabase, "perfectlygenericitem");
  ASSERT_TRUE(trash);
  List<std::pair<InventorySlot, std::shared_ptr<Item>>> items{
    {EquipmentSlot::Head, std::move(*head)},
    {bagSlot, bagItem},
    {SwapSlot(), std::move(*swap)},
    {TrashSlot(), std::move(*trash)}
  };
  for (auto const& entry : items)
    ASSERT_EQ(inventory.setItem(entry.first, entry.second), true);
  ASSERT_TRUE(inventory.set_custom_bar_primary_slot(0, InventorySlot(bagSlot)));
  ASSERT_TRUE(inventory.select_action_bar_location(CustomBarIndex(0)));

  std::vector<InventorySlot> visited_slots;
  ASSERT_TRUE(view.for_each_item([&](InventorySlot const& position, std::shared_ptr<Item> const&) -> std::expected<void, StarException> {
    visited_slots.push_back(position);
    return {};
  }));
  ASSERT_EQ(visited_slots.size(), items.size());
  for (std::size_t i = 0; i < items.size(); ++i)
    EXPECT_EQ(visited_slots[i], items[i].first);
  EXPECT_EQ(view.primary_held_item(), items[2].second);
  EXPECT_EQ(view.secondary_held_item(), nullptr);

  List<long> owners;
  for (auto const& entry : items) {
    ASSERT_TRUE(entry.second->consume(1));
    owners.push_back(entry.second.use_count());
    EXPECT_EQ(view.itemsAt(entry.first), nullptr);
  }

  EXPECT_FALSE(view.headArmor());
  EXPECT_FALSE(view.equipment(EquipmentSlot::Head));
  auto bag = view.bagContents(bagSlot.bag_name);
  ASSERT_TRUE(bag);
  EXPECT_EQ(bag->at(bagSlot.index), nullptr);
  EXPECT_FALSE(view.swapSlotItem());
  EXPECT_EQ(view.primary_held_item(), nullptr);
  EXPECT_EQ(view.secondary_held_item(), nullptr);
  auto primary_held_slot = view.primary_held_slot();
  ASSERT_TRUE(primary_held_slot);
  EXPECT_EQ(*primary_held_slot, std::optional<InventorySlot>(InventorySlot(bagSlot)));
  auto all = view.allItems();
  ASSERT_TRUE(all);
  EXPECT_TRUE(all->empty());
  auto summary = view.itemSummary();
  ASSERT_TRUE(summary);
  EXPECT_TRUE(summary->empty());
  auto available = view.availableItems();
  ASSERT_TRUE(available);
  EXPECT_TRUE(available->by_name.empty());
  EXPECT_TRUE(available->by_parameters.empty());
  std::size_t visited = 0;
  ASSERT_TRUE(view.for_each_item([&](InventorySlot const&, std::shared_ptr<Item> const&) -> std::expected<void, StarException> {
    ++visited;
    return {};
  }));
  EXPECT_EQ(visited, 0u);

  // Restoring a shared alias makes the same slots visible again: no getter
  // released the inventory's ownership of the exhausted items.
  for (std::size_t i = 0; i < items.size(); ++i) {
    auto const& entry = items[i];
    EXPECT_EQ(entry.second.use_count(), owners[i]);
    entry.second->set_count(1);
    EXPECT_EQ(view.itemsAt(entry.first), entry.second);
  }
  {
    auto restoredItems = view.allItems();
    ASSERT_TRUE(restoredItems);
    EXPECT_EQ(restoredItems->size(), items.size());
  }

  for (auto const& entry : items)
    ASSERT_TRUE(entry.second->consume(1));
  ASSERT_TRUE(inventory.cleanup());
  for (std::size_t i = 0; i < items.size(); ++i)
    EXPECT_EQ(items[i].second.use_count(), owners[i] - 1);
  auto custom_bar_primary_slot = view.custom_bar_primary_slot(0);
  ASSERT_TRUE(custom_bar_primary_slot);
  EXPECT_FALSE(*custom_bar_primary_slot);
});

const GameTestRegistration inventoryMutationNormalizesEmptySlots("ItemTest", "InventoryMutationNormalizesEmptySlots", [](GameTest& fixture) {
  auto itemDatabase = fixture.root().itemDatabase();
  ASSERT_TRUE(itemDatabase);
  auto createdInventory = PlayerInventory::create(fixture.root());
  ASSERT_TRUE(createdInventory);
  auto& inventory = **createdInventory;
  auto makeItems = [&](std::uint64_t count) {
    return inventoryTestItem(**itemDatabase, "perfectlygenericitem", count);
  };

  auto itemResult = makeItems(2);
  ASSERT_TRUE(itemResult);
  auto items = std::move(*itemResult);
  auto slotResult = inventoryTestBagSlot(fixture.root(), inventory, items);
  ASSERT_TRUE(slotResult);
  auto bagSlot = *slotResult;
  ASSERT_EQ(inventory.setItem(bagSlot, items), true);
  EXPECT_EQ(inventory.consumeSlot(bagSlot, 2), true);
  EXPECT_EQ(inventory.itemsAt(bagSlot), nullptr);
  EXPECT_EQ(items.use_count(), 1);
  EXPECT_EQ(inventory.consumeSlot(bagSlot), false);
  EXPECT_EQ(inventory.consumeSlot(bagSlot, 0), true);

  auto consumedResult = makeItems(1);
  ASSERT_TRUE(consumedResult);
  auto externallyConsumed = std::move(*consumedResult);
  ASSERT_EQ(inventory.setItem(bagSlot, externallyConsumed), true);
  ASSERT_TRUE(externallyConsumed->consume(1));
  {
    auto addition = makeItems(3);
    ASSERT_TRUE(addition);
    EXPECT_EQ(inventory.stackWith(bagSlot, std::move(*addition)), nullptr);
    auto current = inventory.itemsAt(bagSlot);
    ASSERT_TRUE(current);
    ASSERT_TRUE(*current);
    EXPECT_EQ((*current)->count(), 3u);
  }
  EXPECT_EQ(externallyConsumed.use_count(), 1);
  {
    auto addition = makeItems(2);
    ASSERT_TRUE(addition);
    EXPECT_EQ(inventory.stackWith(bagSlot, std::move(*addition)), nullptr);
    auto current = inventory.itemsAt(bagSlot);
    ASSERT_TRUE(current);
    ASSERT_TRUE(*current);
    EXPECT_EQ((*current)->count(), 5u);
  }
  auto fiveItems = ItemDescriptor::create("perfectlygenericitem", 5);
  ASSERT_TRUE(fiveItems);
  EXPECT_TRUE(inventory.consumeItems(*fiveItems));
  EXPECT_EQ(inventory.itemsAt(bagSlot), nullptr);

  auto armorResult = inventoryTestHeadArmor(fixture.root());
  ASSERT_TRUE(armorResult);
  auto armor = std::move(*armorResult);
  ASSERT_TRUE(armor);
  auto armorDescriptor = armor->descriptor();
  ASSERT_EQ(inventory.setItem(EquipmentSlot::Head, armor), true);
  EXPECT_EQ(inventory.exchangeItems(EquipmentSlot::Head, TrashSlot()), true);
  EXPECT_EQ(inventory.itemsAt(EquipmentSlot::Head), nullptr);
  EXPECT_EQ(inventory.itemsAt(TrashSlot()), armor);
  EXPECT_EQ(inventory.exchangeItems(EquipmentSlot::Head, TrashSlot()), true);
  EXPECT_EQ(inventory.itemsAt(EquipmentSlot::Head), armor);
  {
    auto trash = makeItems(1);
    ASSERT_TRUE(trash);
    ASSERT_EQ(inventory.setItem(TrashSlot(), std::move(*trash)), true);
  }
  EXPECT_EQ(inventory.exchangeItems(EquipmentSlot::Head, TrashSlot()), false);
  EXPECT_EQ(inventory.itemsAt(EquipmentSlot::Head), armor);

  {
    auto swap = makeItems(1);
    ASSERT_TRUE(swap);
    ASSERT_EQ(inventory.setItem(SwapSlot(), std::move(*swap)), true);
  }
  ASSERT_EQ(inventory.consumeSlot(SwapSlot()), true);
  EXPECT_FALSE(inventory.swapSlotItem());
  ASSERT_TRUE(inventory.shiftSwap(EquipmentSlot::Head));
  EXPECT_EQ(inventory.swapSlotItem(), armor);
  EXPECT_FALSE(inventory.headArmor());
  EXPECT_EQ(inventory.clearSwap(), true);
  auto equippedResult = inventory.itemsAt(EquipmentSlot::Head);
  ASSERT_TRUE(equippedResult);
  auto equippedArmor = std::move(*equippedResult);
  ASSERT_TRUE(equippedArmor);
  EXPECT_EQ(equippedArmor->descriptor(), armorDescriptor);
  EXPECT_TRUE(armor->empty());

  ASSERT_TRUE(inventory.consumeItems(armorDescriptor));
  EXPECT_FALSE(inventory.headArmor());
  EXPECT_TRUE(equippedArmor->empty());
  EXPECT_EQ(equippedArmor.use_count(), 1);
  EXPECT_EQ(armor.use_count(), 1);
  auto oneItem = ItemDescriptor::create("perfectlygenericitem", 1);
  ASSERT_TRUE(oneItem);
  ASSERT_TRUE(inventory.consumeItems(*oneItem));
  EXPECT_EQ(inventory.itemsAt(TrashSlot()), nullptr);
  auto takenResult = makeItems(1);
  ASSERT_TRUE(takenResult);
  auto takenEmpty = std::move(*takenResult);
  ASSERT_EQ(inventory.setItem(TrashSlot(), takenEmpty), true);
  ASSERT_TRUE(takenEmpty->consume(1));
  EXPECT_EQ(inventory.takeSlot(TrashSlot()), nullptr);
  EXPECT_EQ(takenEmpty.use_count(), 1);

  auto emptyResult = makeItems(1);
  ASSERT_TRUE(emptyResult);
  auto empty = std::move(*emptyResult);
  ASSERT_TRUE(empty->consume(1));
  EXPECT_EQ(inventory.setItem(EquipmentSlot::Head, empty), true);
  EXPECT_EQ(inventory.setItem(bagSlot, empty), true);
  EXPECT_EQ(inventory.setItem(TrashSlot(), empty), true);
  ASSERT_TRUE(inventory.setSwapSlotItem(empty));
  EXPECT_EQ(empty.use_count(), 1);
  auto all = inventory.allItems();
  ASSERT_TRUE(all);
  EXPECT_TRUE(all->empty());
  EXPECT_EQ(inventory.exchangeItems(EquipmentSlot::Head, TrashSlot()), true);

  auto fail_item = makeItems(1);
  ASSERT_TRUE(fail_item);
  ASSERT_EQ(inventory.setItem(bagSlot, *fail_item), true);
  auto failed = inventory.for_each_item([&](InventorySlot const& position, std::shared_ptr<Item>& current) -> std::expected<void, StarException> {
    EXPECT_EQ(position, InventorySlot(bagSlot));
    EXPECT_TRUE(current->consume(1));
    return std::unexpected(ItemException("stop traversal"));
  });
  EXPECT_FALSE(failed);
  EXPECT_EQ(fail_item->use_count(), 1);
  (*fail_item)->set_count(1);
  EXPECT_EQ(inventory.itemsAt(bagSlot), nullptr);

  auto missingRead = std::as_const(inventory).itemsAt(BagSlot("missingInventoryTestBag", 0));
  ASSERT_FALSE(missingRead);
  EXPECT_TRUE(missingRead.error().is<ItemException>());
  auto missingTake = inventory.takeSlot(BagSlot("missingInventoryTestBag", 0));
  ASSERT_FALSE(missingTake);
  EXPECT_TRUE(missingTake.error().is<ItemException>());
  auto bag = inventory.bagContents(bagSlot.bag_name);
  ASSERT_TRUE(bag);
  auto bagSize = bag->size();
  ASSERT_LE(bagSize, std::numeric_limits<std::uint8_t>::max());
  BagSlot pastEnd(bagSlot.bag_name, static_cast<std::uint8_t>(bagSize));
  auto pastEndRead = std::as_const(inventory).itemsAt(pastEnd);
  ASSERT_FALSE(pastEndRead);
  EXPECT_TRUE(pastEndRead.error().is<OutOfRangeException>());
  auto pastEndTake = inventory.takeSlot(pastEnd);
  ASSERT_FALSE(pastEndTake);
  EXPECT_TRUE(pastEndTake.error().is<OutOfRangeException>());
});

namespace {

auto layout_inventory(Root& root, Json groups, Json indexes, String bag_name, Json bag_size) {
  Json config = JsonObject{{"customBarGroups", std::move(groups)}, {"customBarIndexes", std::move(indexes)},
    {"itemBags", JsonObject{{std::move(bag_name), JsonObject{{"size", std::move(bag_size)}}}}}};
  return NetElementTop<PlayerInventory>::create(root, std::move(config), Json(JsonObject{}));
}

}

const GameTestRegistration inventory_layout("ItemTest", "InventoryLayoutCountsAndLostSaveLinks", [](GameTest& fixture) {
  auto& root = fixture.root();
  for (std::string_view name : {"equipment", "swap", "trash"})
    EXPECT_FALSE(layout_inventory(root, 1, 1, String(name), 1));
  for (Json invalid : {Json(-1), Json(257), Json(0.5), Json("1")}) {
    EXPECT_FALSE(layout_inventory(root, invalid, 1, "bag", 1));
    EXPECT_FALSE(layout_inventory(root, 1, invalid, "bag", 1));
    EXPECT_FALSE(layout_inventory(root, 1, 1, "bag", invalid));
  }
  EXPECT_FALSE(layout_inventory(root, 0, 1, "bag", 1));
  EXPECT_FALSE(layout_inventory(root, 1, 0, "bag", 1));
  auto many_groups = layout_inventory(root, 256, 1, "bag", 0);
  ASSERT_TRUE(many_groups);
  EXPECT_EQ((*many_groups)->custom_bar_groups(), 256u);
  EXPECT_TRUE((*many_groups)->set_custom_bar_group(255));
  EXPECT_FALSE((*many_groups)->set_custom_bar_group(256));
  EXPECT_EQ((*many_groups)->custom_bar_group(), 255u);

  auto original = layout_inventory(root, 2, 256, "bag", 256);
  ASSERT_TRUE(original);
  auto& inventory = **original;
  EXPECT_EQ(inventory.custom_bar_indexes(), 256u);
  ASSERT_TRUE(inventory.set_custom_bar_group(1));
  ASSERT_TRUE(inventory.select_action_bar_location(CustomBarIndex{255}));
  EXPECT_FALSE(inventory.set_custom_bar_primary_slot(256, {}));
  auto database = root.itemDatabase();
  ASSERT_TRUE(database);
  auto item = inventoryTestItem(**database, "perfectlygenericitem", 3,
      JsonObject{{"inventoryFilters", JsonObject{{"default", JsonObject{}}}}});
  ASSERT_TRUE(item);
  auto blocker = inventoryTestItem(**database, "perfectlygenericitem", 1,
      JsonObject{{"inventoryFilters", JsonObject{{"default", JsonObject{}}}}, {"marker", "keep"}});
  ASSERT_TRUE(blocker);
  ASSERT_EQ(inventory.setItem(BagSlot{"bag", 0}, *blocker), true);
  ASSERT_EQ(inventory.setItem(BagSlot{"bag", 255}, *item), true);
  ASSERT_TRUE(inventory.set_custom_bar_primary_slot(255, BagSlot{"bag", 255}));
  ASSERT_EQ(inventory.primary_held_item(), *item);
  ASSERT_TRUE(inventory.set_custom_bar_group(0));
  ASSERT_TRUE(inventory.set_custom_bar_primary_slot(0, BagSlot{"bag", 255}));
  ASSERT_TRUE(inventory.set_custom_bar_group(1));
  inventory.setEssentialItem(EssentialItem::WireTool, *item);
  auto save = inventory.store();
  ASSERT_TRUE(save);
  EXPECT_EQ(save->get_uint("customBarGroup"), 1u);
  EXPECT_EQ(save->get_uint("selectedActionBar"), 255u);
  EXPECT_EQ(save->contains("wireTool"), true);
  EXPECT_EQ(save->contains("wiretool"), false);

  auto smaller = layout_inventory(root, 1, 1, "bag", 1);
  ASSERT_TRUE(smaller);
  auto& recovered = **smaller;
  ASSERT_TRUE(recovered.load(*save));
  EXPECT_EQ(recovered.custom_bar_group(), 0u);
  EXPECT_EQ(recovered.selected_action_bar_location(), SelectedActionBarLocation{});
  auto cleared = recovered.custom_bar_primary_slot(0);
  ASSERT_TRUE(cleared);
  EXPECT_FALSE(*cleared);
  auto retained = recovered.itemsAt(BagSlot{"bag", 0});
  ASSERT_TRUE(retained && *retained);
  EXPECT_EQ((*retained)->descriptor(), (*blocker)->descriptor());
  auto overflow = recovered.pullOverflow();
  ASSERT_EQ(overflow.size(), 1u);
  EXPECT_EQ(overflow.front()->count(), 3u);
  ASSERT_TRUE(recovered.select_action_bar_location(EssentialItem::WireTool));
  auto held = recovered.primary_held_item();
  ASSERT_TRUE(held && *held);
  EXPECT_EQ((*held)->descriptor(), (*item)->descriptor());
  EXPECT_EQ(recovered.secondary_held_item(), nullptr);
  EXPECT_FALSE(recovered.select_action_bar_location(CustomBarIndex{1}));
  EXPECT_EQ(recovered.selected_action_bar_location(), SelectedActionBarLocation(EssentialItem::WireTool));

  for (Json invalid : {Json(-1), Json(256), Json(0.25)}) {
    for (std::string_view key : {"customBarGroup", "selectedActionBar"}) {
      auto broken = save->set(String(key), invalid);
      ASSERT_TRUE(broken);
      EXPECT_FALSE(recovered.load(*broken));
    }
    auto broken = save->set("customBar", JsonArray{JsonArray{JsonArray{JsonObject{{"type", "bag"}, {"location", invalid}}}}});
    ASSERT_TRUE(broken);
    EXPECT_FALSE(recovered.load(*broken));
  }
});

const GameTestRegistration inventory_net_layout("ItemTest", "InventoryNetworkLayoutAndLegacyOrdinals", [](GameTest& fixture) {
  // No bag elements, one custom link. Version 9 inserts twelve equipment elements
  // ahead of swap/trash/currencies/group. Field numbers in deltas are one-based.
  for (std::uint32_t version : {8u, 9u}) {
    NetCompatibilityRules rules(version);
    auto created = layout_inventory(fixture.root(), 1, 1, "bag", 0);
    ASSERT_TRUE(created);
    auto& inventory = **created;
    auto baseline = inventory.write_net_state(0, rules);
    ASSERT_TRUE(baseline);
    auto head = inventoryTestHeadArmor(fixture.root());
    ASSERT_TRUE(head);
    ASSERT_EQ(inventory.setItem(EquipmentSlot::HeadCosmetic, *head), true);
    ASSERT_EQ(inventory.setItem(EquipmentSlot::Cosmetic12, *head), true);
    auto delta = inventory.write_net_state(baseline->second, rules);
    ASSERT_TRUE(delta);
    DataStreamBuffer wire(std::move(delta->first));
    wire.set_stream_compatibility_version(rules);
    ASSERT_EQ(wire.read<bool>(), false);
    ASSERT_EQ(wire.read_vlq_u(), 5u);
    EXPECT_EQ(wire.read<ItemDescriptor>(), (*head)->descriptor());
    if (version >= 9) {
      ASSERT_EQ(wire.read_vlq_u(), 20u);
      EXPECT_EQ(wire.read<ItemDescriptor>(), (*head)->descriptor());
    }
    EXPECT_EQ(wire.read_vlq_u(), 0u);

    std::uint64_t group_field = version >= 9 ? 24 : 12;
    // Each invalid packet is followed by a valid full snapshot to restore the
    // wire fields. Rejected values must never be published or alias index 0.
    for (unsigned invalid = 0; invalid < 5; ++invalid) {
      DataStreamBuffer packet;
      ASSERT_TRUE(packet.write(false));
      if (invalid < 2) {
        ASSERT_TRUE(packet.write_vlq_u(group_field));
        ASSERT_TRUE(packet.write_vlq_u(invalid == 0 ? 1 : 256));
      } else if (invalid == 2) {
        ASSERT_TRUE(packet.write_vlq_u(group_field + 1));
        ASSERT_TRUE(packet.write(CustomBarLink{BagSlot{"bag", 0}, {}}));
      } else {
        ASSERT_TRUE(packet.write_vlq_u(group_field + 2));
        auto selected = invalid == 3 ? SelectedActionBarLocation(CustomBarIndex{1})
            : SelectedActionBarLocation(static_cast<EssentialItem>(255));
        ASSERT_TRUE(packet.write(selected));
      }
      ASSERT_TRUE(packet.write_vlq_u(0));
      auto rejected = inventory.read_net_state(packet.take_data(), 0.0f, rules);
      EXPECT_FALSE(rejected);
      EXPECT_EQ(inventory.custom_bar_group(), 0u);
      EXPECT_EQ(inventory.selected_action_bar_location(), SelectedActionBarLocation{});
      auto link = inventory.custom_bar_primary_slot(0);
      ASSERT_TRUE(link);
      EXPECT_FALSE(*link);
      ASSERT_TRUE(inventory.read_net_state(baseline->first, 0.0f, rules));
    }
  }
});

const GameTestRegistration inventory_delta_visibility(
    "ItemTest", "CachedReplicaInventoryDeltaRespectsRecipient", [](GameTest& fixture) {
  auto& root = fixture.root();
  auto storage = File::ephemeral_file(root.random());
  ASSERT_TRUE(storage);
  auto world = WorldServer::create(root, Vec2U(2048, 2048), *storage);
  ASSERT_TRUE(world);
  auto close = make_scope_exit([&] { EXPECT_TRUE((*world)->close()); });
  auto factory = root.playerFactory();
  ASSERT_TRUE(factory);
  auto player = (*factory)->create();
  ASSERT_TRUE(player);
  ASSERT_TRUE((*player)->finalizeCreation());
  Entity& entity = **player;
  ASSERT_TRUE(entity.init(**world, 100, EntityMode::Slave));
  auto uninit = make_scope_exit([&] { EXPECT_TRUE(entity.uninit()); });
  auto database = root.itemDatabase();
  ASSERT_TRUE(database);

  for (bool admin_first : {false, true}) {
    auto source = NetElementTop<PlayerInventory>::create(root);
    ASSERT_TRUE(source);
    (*source)->setPlayer(std::ref(**player));
    NetCompatibilityRules privileged(open_protocol_version, true);
    auto initial = (*source)->write_net_state(0, privileged);
    ASSERT_TRUE(initial);
    auto item = inventoryTestItem(**database, "perfectlygenericitem", 2);
    ASSERT_TRUE(item);
    ASSERT_EQ((*source)->setItem(SwapSlot(), std::move(*item)), true);

    // One entity and one starting cursor: only recipient policy may differ.
    HashMap<NetCompatibilityRules, ByteArray> deltas;
    for (bool admin : {admin_first, !admin_first}) {
      NetCompatibilityRules rules(open_protocol_version, admin);
      if (!deltas.contains(rules)) {
        auto delta = (*source)->write_net_state(initial->second, rules);
        ASSERT_TRUE(delta);
        deltas[rules] = std::move(delta->first);
      }
      auto observer = NetElementTop<PlayerInventory>::create(root);
      ASSERT_TRUE(observer);
      ASSERT_TRUE((*observer)->read_net_state(initial->first, 0.0f, rules));
      ASSERT_TRUE((*observer)->read_net_state(deltas[rules], 0.0f, rules));
      auto received = (*observer)->swapSlotItem();
      EXPECT_EQ(received ? received->count() : 0u, admin ? 2u : 0u);
    }
  }
});

const GameTestRegistration inventoryMutableTraversalAndNetworkNormalizeConsumption("ItemTest", "InventoryMutableTraversalAndNetworkNormalizeConsumption", [](GameTest& fixture) {
  auto itemDatabase = fixture.root().itemDatabase();
  ASSERT_TRUE(itemDatabase);
  auto masterResult = NetElementTop<PlayerInventory>::create(fixture.root());
  ASSERT_TRUE(masterResult);
  auto& master = **masterResult;
  auto slaveResult = NetElementTop<PlayerInventory>::create(fixture.root());
  ASSERT_TRUE(slaveResult);
  auto& slave = **slaveResult;
  auto itemResult = inventoryTestItem(**itemDatabase, "perfectlygenericitem", 2);
  ASSERT_TRUE(itemResult);
  auto item = std::move(*itemResult);
  auto slotResult = inventoryTestBagSlot(fixture.root(), master, item);
  ASSERT_TRUE(slotResult);
  auto bagSlot = *slotResult;
  ASSERT_EQ(master.setItem(bagSlot, item), true);
  {
    auto trash = inventoryTestItem(**itemDatabase, "perfectlygenericitem");
    ASSERT_TRUE(trash);
    ASSERT_EQ(master.setItem(TrashSlot(), std::move(*trash)), true);
  }
  ASSERT_TRUE(master.set_custom_bar_primary_slot(0, InventorySlot(bagSlot)));

  auto initial = master.write_net_state();
  ASSERT_TRUE(initial);
  ASSERT_TRUE(slave.read_net_state(initial->first));
  {
    auto current = slave.itemsAt(bagSlot);
    ASSERT_TRUE(current);
    ASSERT_TRUE(*current);
    EXPECT_EQ((*current)->count(), 2u);
    auto trash = slave.itemsAt(TrashSlot());
    ASSERT_TRUE(trash);
    EXPECT_TRUE(*trash);
  }

  std::size_t visited = 0;
  ASSERT_TRUE(master.for_each_item([&](InventorySlot const&, std::shared_ptr<Item>& current) -> std::expected<void, StarException> {
    ++visited;
    EXPECT_TRUE(current->consume(current->count()));
    return {};
  }));
  EXPECT_EQ(visited, 2u);
  EXPECT_EQ(item.use_count(), 1);
  auto masterItems = master.allItems();
  ASSERT_TRUE(masterItems);
  EXPECT_TRUE(masterItems->empty());
  auto delta = master.write_net_state(initial->second);
  ASSERT_TRUE(delta);
  ASSERT_TRUE(slave.read_net_state(delta->first));
  auto slaveItems = slave.allItems();
  ASSERT_TRUE(slaveItems);
  EXPECT_TRUE(slaveItems->empty());
  auto slaveCustomBarPrimarySlot = slave.custom_bar_primary_slot(0);
  ASSERT_TRUE(slaveCustomBarPrimarySlot);
  EXPECT_FALSE(*slaveCustomBarPrimarySlot);

  {
    auto swap = inventoryTestItem(**itemDatabase, "perfectlygenericitem");
    ASSERT_TRUE(swap);
    ASSERT_EQ(master.setItem(SwapSlot(), std::move(*swap)), true);
  }
  auto liveSwap = master.write_net_state(delta->second);
  ASSERT_TRUE(liveSwap);
  ASSERT_TRUE(slave.read_net_state(liveSwap->first));
  EXPECT_TRUE(slave.swapSlotItem());
  auto swapAlias = master.swapSlotItem();
  ASSERT_TRUE(swapAlias);
  ASSERT_TRUE(swapAlias->consume(1));
  auto emptySwap = master.write_net_state(liveSwap->second);
  ASSERT_TRUE(emptySwap);
  ASSERT_TRUE(slave.read_net_state(emptySwap->first));
  EXPECT_FALSE(slave.swapSlotItem());
  EXPECT_EQ(swapAlias.use_count(), 1);

  auto restoredResult = PlayerInventory::create(fixture.root());
  ASSERT_TRUE(restoredResult);
  auto& restored = **restoredResult;
  auto stored = master.store();
  ASSERT_TRUE(stored);
  ASSERT_TRUE(restored.load(*stored));
  auto restoredItems = restored.allItems();
  ASSERT_TRUE(restoredItems);
  EXPECT_TRUE(restoredItems->empty());
});

namespace {

class ReadyCatalogErrorSink : public LogSink {
public:
  ReadyCatalogErrorSink() { set_level(LogLevel::Error); }
  [[nodiscard]] std::expected<void, StarException> log(std::string_view message, LogLevel) override {
    ADD_FAILURE() << "Ready-catalog fixture logged an error: " << message;
    return {};
  }
};

}

const GameTestRegistration
  nestedReadyCatalog("ItemTest", "BuildersAndRebuildersKeepReadyCatalogAcrossNestedServices", [](GameTest& fixture) {
    auto temporaryDirectory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(temporaryDirectory);
    auto directory = std::move(*temporaryDirectory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(directory)); });
    auto overlay = File::relative_to(directory, "assets");
    auto writeAsset = [&](String const& path, String const& contents) -> std::expected<void, StarException> {
      auto absolute = path.starts_with("/");
      if (!absolute)
        return std::unexpected(std::move(absolute.error()));
      String relative = path;
      if (*absolute) {
        auto suffix = path.substr(1);
        if (!suffix)
          return std::unexpected(std::move(suffix.error()));
        relative = std::move(*suffix);
      }
      auto destination = File::relative_to(overlay, relative);
      auto makeDirectory = File::create_directories(File::dir_name(destination));
      if (!makeDirectory)
        return std::unexpected(std::move(makeDirectory.error()));
      return File::write_file(contents, destination);
    };

    // Reuse the actual fixture's legal species/NPC assets, including their real
    // creation/build scripts. The wrappers add observable nested item consumers.
    auto assetResult = fixture.root().assets();
    ASSERT_TRUE(assetResult);
    auto const& baseAssets = *assetResult;
    String humanPath;
    Json humanConfig;
    auto speciesPaths = baseAssets->scan_extension("species");
    ASSERT_TRUE(speciesPaths);
    for (auto const& assetPath : speciesPaths->get()) {
      auto const& path = assetPath.string();
      auto config = baseAssets->json(path);
      ASSERT_TRUE(config);
      auto kind = config->get_string("kind");
      ASSERT_TRUE(kind);
      if (*kind == "human") {
        humanPath = path;
        humanConfig = std::move(*config);
        break;
      }
    }
    ASSERT_FALSE(humanPath.empty());
    Json npcConfig;
    auto npcPaths = baseAssets->scan_extension("npctype");
    ASSERT_TRUE(npcPaths);
    for (auto const& assetPath : npcPaths->get()) {
      auto const& path = assetPath.string();
      auto config = baseAssets->json(path);
      ASSERT_TRUE(config);
      auto type = config->get_string("type");
      ASSERT_TRUE(type);
      if (*type == "villager") {
        npcConfig = std::move(*config);
        break;
      }
    }
    ASSERT_FALSE(npcConfig.is_null());
    auto creationScripts = humanConfig.get_array("createScripts", {});
    ASSERT_TRUE(creationScripts);
    if (creationScripts->empty())
      creationScripts->push_back("/humanoid/asterbound/create.lua");
    creationScripts->push_back("/readycatalog/create.lua");
    auto buildScripts = humanConfig.get_array("buildScripts", {});
    ASSERT_TRUE(buildScripts);
    if (buildScripts->empty())
      buildScripts->push_back("/humanoid/asterbound/build.lua");
    buildScripts->push_back("/readycatalog/humanoid.lua");
    auto humanWithCreation = humanConfig.set("createScripts", *creationScripts);
    ASSERT_TRUE(humanWithCreation);
    auto humanWithBuild = humanWithCreation->set("buildScripts", *buildScripts);
    ASSERT_TRUE(humanWithBuild);
    humanConfig = std::move(*humanWithBuild);
    auto humanContents = humanConfig.repr();
    ASSERT_TRUE(humanContents);
    ASSERT_TRUE(writeAsset(humanPath, *humanContents));
    auto npcType = npcConfig.set("type", "readycatalognpc");
    ASSERT_TRUE(npcType);
    auto npcHumanoid = npcType->set("humanoidConfig", "/humanoid.config");
    ASSERT_TRUE(npcHumanoid);
    auto npcItems = Json::parse(R"({"override":[[0,[{}]]]})");
    ASSERT_TRUE(npcItems);
    auto npcWithItems = npcHumanoid->set("items", *npcItems);
    ASSERT_TRUE(npcWithItems);
    npcConfig = std::move(*npcWithItems);
    auto npcContents = npcConfig.repr();
    ASSERT_TRUE(npcContents);
    ASSERT_TRUE(writeAsset("/readycatalog/probe.npctype", *npcContents));
    ASSERT_TRUE(writeAsset("/readycatalog/create.lua", R"(
local originalCreate = create
function create(...)
  local identity, parameters, armor = originalCreate(...)
  local descriptor = root.createItem("readycatalogleaf")
  assert(descriptor.name == "readycatalogleaf")
  parameters.readyCreateMarker = root.itemConfig("readycatalogleaf").config.shortdescription
  return identity, parameters, armor
end
)"));
    ASSERT_TRUE(writeAsset("/readycatalog/humanoid.lua", R"(
local originalBuild = build
function build(...)
  local config = originalBuild(...)
  local descriptor = root.createItem("readycatalogleaf")
  assert(descriptor.name == "readycatalogleaf")
  root.setConfiguration("readySpeciesBuild", root.itemConfig("readycatalogleaf").config.shortdescription)
  return config
end
)"));

    ASSERT_TRUE(writeAsset("/_metadata", R"({
    "name":"ready_catalog_fixture",
    "errorHandlers":{"item":["/readycatalog/rebuild.lua"]}
  })"));
    ASSERT_TRUE(writeAsset("/readycatalog/leaf.item", R"({
    "itemName":"readycatalogleaf","rarity":"common",
    "shortdescription":"LeafOwner","description":"Ready catalog leaf"
  })"));
    ASSERT_TRUE(writeAsset("/readycatalog/built.item", R"({
    "itemName":"readycatalogbuilt","rarity":"common",
    "shortdescription":"Unbuilt","builder":"/readycatalog/build.lua"
  })"));
    ASSERT_TRUE(writeAsset("/readycatalog/broken.item", R"({
    "itemName":"readycatalogbroken","rarity":"common",
    "shortdescription":"Unrebuilt","builder":"/readycatalog/broken.lua"
  })"));
    ASSERT_TRUE(writeAsset("/readycatalog/broken.lua", R"(
function build()
  error("intentional ready-catalog rebuild")
end
)"));
    ASSERT_TRUE(writeAsset("/readycatalog/probe.treasurepools", R"({
    "readycatalogpool":[[0,{"fill":[{"item":"readycatalogleaf"}],"poolRounds":0}]]
  })"));
    ASSERT_TRUE(writeAsset("/readycatalog/probe.collection", R"({
    "name":"readycatalogcollection","type":"item",
    "collectables":{"leaf":{"item":"readycatalogleaf","icon":"/assetmissing.png"}}
  })"));
    auto versioningConfig = baseAssets->json("/versioning.config");
    ASSERT_TRUE(versioningConfig);
    auto updatedVersioning = versioningConfig->set("ReadyCatalog", 1);
    ASSERT_TRUE(updatedVersioning);
    auto versioningContents = updatedVersioning->repr();
    ASSERT_TRUE(versioningContents);
    ASSERT_TRUE(writeAsset("/versioning.config", *versioningContents));
    ASSERT_TRUE(writeAsset("/versioning/ReadyCatalog_0_1.lua", R"(
function update(data)
  return {
    name = root.createItem("readycatalogleaf").name,
    marker = root.itemConfig("readycatalogleaf").config.shortdescription
  }
end
)"));

    String checkNestedServices = R"(
local function checkNestedServices()
  local leaf = root.itemConfig("readycatalogleaf").config.shortdescription
  assert(leaf == "LeafOwner")
  assert(root.createItem("readycatalogleaf").name == "readycatalogleaf")
  local treasure = root.createTreasure("readycatalogpool", 1, 123)
  assert(#treasure == 1 and treasure[1].name == "readycatalogleaf")
  local upgraded = root.loadVersionedJson(
    {id="ReadyCatalog",version=0,content={}}, "ReadyCatalog")
  assert(upgraded.name == "readycatalogleaf" and upgraded.marker == leaf)
  assert(root.collection("readycatalogcollection").name == "readycatalogcollection")
  local collectables = root.collectables("readycatalogcollection")
  assert(#collectables == 1 and collectables[1].title == leaf)
  local variant = root.npcVariant("human", "readycatalognpc", 1, 456)
  assert(variant.humanoidParameters.readyCreateMarker == leaf)
  local portrait = root.npcPortrait("head", "human", "readycatalognpc", 1, 456)
  assert(#portrait > 0)
  assert(root.getConfiguration("readySpeciesBuild") == leaf)
  return leaf
end
)";
    ASSERT_TRUE(writeAsset("/readycatalog/build.lua", checkNestedServices + R"(
function build(directory, config, parameters)
  config.shortdescription = "NestedReady_" .. checkNestedServices()
  parameters.nestedCatalog = "builder"
  return config, parameters
end
)"));
    ASSERT_TRUE(writeAsset("/readycatalog/rebuild.lua", checkNestedServices + R"(
function error(descriptor, message)
  if descriptor.name ~= "readycatalogbroken" then return descriptor end
  assert(string.find(message, "intentional ready%-catalog rebuild"))
  return root.createItem({
    name = "readycatalogleaf",
    count = descriptor.count,
    parameters = {
      shortdescription = "RebuiltReady_" .. checkNestedServices(),
      nestedCatalog = "rebuilder"
    }
  })
end
)"));
    ASSERT_TRUE(writeAsset("/readycatalog/built.recipe", R"({
    "input":[],"output":{"name":"readycatalogbuilt","count":1},
    "groups":[],"collectables":{}
  })"));
    ASSERT_TRUE(writeAsset("/readycatalog/broken.recipe", R"({
    "input":[],"output":{"name":"readycatalogbroken","count":1},
    "groups":[],"collectables":{}
  })"));

    auto settings = fixture.rootSettings();
    settings.assetSources.push_back(overlay);
    settings.storageDirectory = File::relative_to(directory, "storage");
    settings.runtimeConfigFile = {};
    settings.logDirectory = {};
    settings.logFile = {};
    settings.includeUGC = false;
    auto createdRoot = Root::create(settings);
    ASSERT_TRUE(createdRoot);
    auto& root = **createdRoot;
    root.logger().add_sink(std::make_shared<ReadyCatalogErrorSink>());

    // These strings are recorded by the INITIAL recipe scan while the Root has
    // not published ItemDatabase. A later db.item() alone could hide a fallback.
    auto databaseResult = root.itemDatabase();
    ASSERT_TRUE(databaseResult);
    auto const& database = *databaseResult;
    auto builtRecipes = database->recipesForOutputItem("readycatalogbuilt");
    ASSERT_EQ(builtRecipes.size(), 1u);
    EXPECT_EQ(builtRecipes.begin()->guiFilterString.contains("nestedready_leafowner"), true);
    auto rebuiltRecipes = database->recipesForOutputItem("readycatalogbroken");
    ASSERT_EQ(rebuiltRecipes.size(), 1u);
    EXPECT_EQ(rebuiltRecipes.begin()->guiFilterString.contains("rebuiltready_leafowner"), true);
    auto builtResult = inventoryTestItem(*database, "readycatalogbuilt");
    ASSERT_TRUE(builtResult);
    auto const& built = *builtResult;
    ASSERT_TRUE(built);
    EXPECT_EQ(built->name(), "readycatalogbuilt");
    EXPECT_EQ(built->friendly_name(), "NestedReady_LeafOwner");
    EXPECT_EQ(built->parameters().get_string("nestedCatalog"), "builder");
    auto rebuiltResult = inventoryTestItem(*database, "readycatalogbroken", 3);
    ASSERT_TRUE(rebuiltResult);
    auto const& rebuilt = *rebuiltResult;
    ASSERT_TRUE(rebuilt);
    EXPECT_EQ(rebuilt->name(), "readycatalogleaf");
    EXPECT_EQ(rebuilt->count(), 3u);
    EXPECT_EQ(rebuilt->friendly_name(), "RebuiltReady_LeafOwner");
    EXPECT_EQ(rebuilt->parameters().get_string("nestedCatalog"), "rebuilder");
    auto collections = root.collection_database();
    ASSERT_TRUE(collections);
    auto leaf = (*collections)->collectable("readycatalogcollection", "leaf");
    ASSERT_TRUE(leaf);
    EXPECT_EQ(leaf->title, "LeafOwner");
    auto configuration = root.configuration();
    ASSERT_TRUE(configuration);
    auto marker = (*configuration)->get("readySpeciesBuild");
    ASSERT_TRUE(marker);
    EXPECT_EQ(marker->to_string(), "LeafOwner");
  });

const GameTestRegistration
  beamPresentationBoundary("ItemTest", "BeamTransitionsBelongToTicksNotDisplaySampling", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto settings = fixture.rootSettings();
    settings.storageDirectory = File::relative_to(*directory, "storage");
    settings.runtimeConfigFile = {};
    settings.logDirectory = {};
    settings.logFile = {};
    settings.includeUGC = false;
    auto createdRoot = Root::create(std::move(settings));
    ASSERT_TRUE(createdRoot);
    auto& root = **createdRoot;
    auto storage = File::ephemeral_file(root.random());
    ASSERT_TRUE(storage);
    auto world = WorldServer::create(root, Vec2U(2048, 2048), *storage);
    ASSERT_TRUE(world) << world.error().what();
    auto close = make_scope_exit([&] { EXPECT_TRUE((*world)->close()); });
    (*world)->setSpawningEnabled(false);
    auto factory = root.playerFactory();
    ASSERT_TRUE(factory);
    auto made = (*factory)->create();
    ASSERT_TRUE(made);
    auto& player = **made;
    ASSERT_TRUE(player.finalizeCreation());
    auto uuid = player.uuid();
    ASSERT_TRUE(uuid);
    auto context = ClientContext::create(root.logger(), Uuid(root.random()), *uuid, 1, {});
    ASSERT_TRUE(context);
    player.set_client_context(*context);
    Entity& entity = player;
    ASSERT_TRUE(entity.init(**world, 100, EntityMode::Master));
    auto uninitPlayer = make_scope_exit([&] { EXPECT_TRUE(entity.uninit()); });
    ASSERT_TRUE(player.refresh_humanoid());
    Vec2F const origin(1024, 1024);
    ASSERT_TRUE(player.moveTo(origin));
    auto tools = ToolUser::create(root);
    ASSERT_TRUE(tools);
    ASSERT_TRUE((*tools)->init(player));
    auto uninitTools = make_scope_exit([&] { EXPECT_TRUE((*tools)->uninit()); });
    auto database = root.itemDatabase();
    ASSERT_TRUE(database);
    auto paint = (*database)->itemConfig("painttool", JsonObject{});
    ASSERT_TRUE(paint);
    // Keep the genuine asset and item implementation; make the cubic overshoot
    // its endpoint, and remove display noise so its geometry is observable.
    auto config = paint->config.set_all(JsonObject{
      {"nearControlPointElasticity", 1}, {"farControlPointElasticity", 1},
      {"nearControlPointDistance", 3}, {"targetSegmentRun", 1},
      {"minBeamLines", 1}, {"maxBeamLines", 1}, {"maxBeamJitter", 0},
      {"minBeamTrans", 0}, {"maxBeamTrans", 1}, {"beamTransDev", 0}});
    ASSERT_TRUE(config);
    auto item = PaintingBeamTool::create(root, *config, paint->directory, paint->parameters);
    ASSERT_TRUE(item);
    BeamItem& beam = **item;
    auto point_at = [](Vec2F const& start, Vec2F const& end, float t) {
      float u = 1 - t;
      // Independent Bernstein form for controls {start, 3*(end-start), end, end}.
      return start * (u * u * u) + (end - start) * (9 * u * u * t) + end * ((3 * u + t) * t * t);
    };

    struct Events : NullRenderCallback {
      std::vector<std::tuple<Vec2F, Vec2F, Color>> particles;
      std::expected<void, StarException> add_particle(Particle&& particle) override {
        if (particle.type == Particle::Type::Ember && particle.timeToLive == 0.25f)
          particles.emplace_back(particle.position, particle.velocity, particle.color);
        return {};
      }
    };
    NullRenderCallback discard;
    std::vector<std::tuple<Vec2F, Vec2F, Color>> baseline;
    std::uint64_t baselineRandom = 0;
    constexpr float dt = 1.f / 60;
    for (unsigned displayCalls : {0u, 1u, 7u}) {
      SCOPED_TRACE(displayCalls);
      ASSERT_TRUE((*tools)->setItems({}, {}));
      ASSERT_TRUE((*tools)->setItems(*item, {}));
      constexpr float range = 25;
      beam.setRange(range);
      player.aim(origin + Vec2F(5, 0));
      // Equipping deliberately triggers the real item's cooldown. Advance
      // fixed ticks through it before requiring a visible painting beam.
      unsigned equipTicks = (unsigned)std::ceil((*item)->cooldownTime() / dt) + 1;
      for (unsigned i = 0; i != equipTicks; ++i)
        ASSERT_TRUE((*tools)->tick(dt, false, {}));
      auto warm = beam.nonRotatedDrawables();
      ASSERT_TRUE(warm);
      ASSERT_FALSE(warm->empty());
      auto first = std::find_if(warm->begin(), warm->end(), [](Drawable const& d) { return d.is_line(); });
      ASSERT_NE(first, warm->end());
      auto const start = first->line_part().line.min();
      auto const end = center_of_tile(origin + Vec2F(5, 0)) - player.position();
      float length = 0;
      Vec2F previous = start;
      for (int i = 1; i <= 512; ++i) {
        auto point = point_at(start, end, static_cast<float>(i) / 512);
        length += (point - previous).magnitude();
        previous = point;
      }
      auto opacity = beam.getAppropriateOpacity();
      ASSERT_TRUE(opacity);
      EXPECT_NEAR(*opacity, 1 - length / range, .002f);
      // Three stripes per segment, no jitter. The first display vertex is
      // uniformly spaced in parameter even though the cubic doubles back.
      auto const stripes = std::distance(first, warm->end());
      ASSERT_EQ(stripes % 3, 0);
      ASSERT_GT(stripes, 3);
      auto const vertex = point_at(start, end, 1.f / static_cast<float>(stripes / 3 + 1));
      EXPECT_LT((first->line_part().line.max() - vertex).magnitude(), .001f);
      ASSERT_TRUE(player.collectRenderEvents(discard));

      // This private Root has no UniverseServer/background simulation threads.
      constexpr std::uint64_t seed = 0x4245414d5449434b;
      root.random().seed(seed);
      RandomSource untouched(seed);
      for (unsigned i = 0; i != displayCalls; ++i) {
        auto sampled = beam.nonRotatedDrawables();
        ASSERT_TRUE(sampled);
        EXPECT_FALSE(sampled->empty());
      }
      EXPECT_EQ(root.random().next_u64(), untouched.next_u64());

      // Changing input is not itself a beam tick. Display sampling must neither
      // observe the future transition nor enqueue its leftovers.
      player.aim(origin + Vec2F(20, 0));
      auto const far_end = center_of_tile(origin + Vec2F(20, 0)) - player.position();
      ASSERT_LT(far_end.magnitude(), range);
      auto hand = player.hand_position(ToolHand::Primary, (beam.firePosition() - beam.handPosition()) / tile_pixels);
      ASSERT_TRUE(hand);
      ASSERT_LT((far_end - *hand).magnitude(), range); // the chord alone would stay in range
      for (unsigned i = 0; i != displayCalls; ++i) {
        auto sampled = beam.nonRotatedDrawables();
        ASSERT_TRUE(sampled);
        EXPECT_FALSE(sampled->empty());
      }
      Events events;
      ASSERT_TRUE(player.collectRenderEvents(events));
      EXPECT_TRUE(events.particles.empty());
      EXPECT_EQ(root.random().next_u64(), untouched.next_u64());

      ASSERT_TRUE((*tools)->tick(dt, false, {}));
      for (unsigned i = 0; i != displayCalls; ++i) {
        auto sampled = beam.nonRotatedDrawables();
        ASSERT_TRUE(sampled);
        EXPECT_TRUE(sampled->empty());
      }
      ASSERT_TRUE(player.collectRenderEvents(events));
      ASSERT_GT(events.particles.size(), 4u);
      // At targetSegmentRun=1 the broken beam deposits approximately two
      // particles per tile of arc. Chords may shorten across the turnaround.
      auto previous_particle = player.position() + *hand;
      float furthest_x = previous_particle[0];
      for (auto const& particle : events.particles) {
        auto const& point = std::get<0>(particle);
        EXPECT_LE((point - previous_particle).magnitude(), .53f);
        furthest_x = std::max(furthest_x, point[0]);
        previous_particle = point;
      }
      EXPECT_NEAR((std::get<0>(events.particles.front()) - (player.position() + *hand)).magnitude(), .5f, .03f);
      EXPECT_GT(furthest_x, player.position()[0] + far_end[0] + 4);
      EXPECT_NEAR((previous_particle - (player.position() + far_end)).magnitude(), 1.f, .05f);
      auto count = events.particles.size();
      ASSERT_TRUE(player.collectRenderEvents(events));
      EXPECT_EQ(events.particles.size(), count);
      ASSERT_TRUE((*tools)->tick(dt, false, {}));
      ASSERT_TRUE(player.collectRenderEvents(events));
      EXPECT_EQ(events.particles.size(), count);
      auto nextRandom = root.random().next_u64();
      if (displayCalls == 0) {
        baseline = events.particles;
        baselineRandom = nextRandom;
      } else {
        EXPECT_EQ(events.particles, baseline);
        EXPECT_EQ(nextRandom, baselineRandom);
      }

      // A hidden tick clears the prior visible range state, not a deferred
      // transition waiting for a later display/reveal.
      player.aim(origin + Vec2F(2, 0));
      ASSERT_TRUE((*tools)->tick(dt, false, {}));
      (*tools)->suppressItems(true);
      player.aim(origin + Vec2F(20, 0));
      ASSERT_TRUE((*tools)->tick(dt, false, {}));
      (*tools)->suppressItems(false);
      ASSERT_TRUE((*tools)->tick(dt, false, {}));
      Events revealed;
      ASSERT_TRUE(player.collectRenderEvents(revealed));
      EXPECT_TRUE(revealed.particles.empty());
    }
  });

namespace {
void with_scripted_item_owner(GameTest& fixture, String const& script, std::invocable<Root&, Player&> auto&& test) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory);
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto overlay = File::relative_to(*directory, "assets");
  ASSERT_TRUE(File::create_directories(overlay));
  ASSERT_TRUE(File::write_file(script, File::relative_to(overlay, "active-item-test.lua")));
  auto settings = fixture.rootSettings();
  settings.assetSources.push_back(overlay);
  settings.storageDirectory = File::relative_to(*directory, "storage");
  settings.runtimeConfigFile = {};
  settings.logDirectory = {};
  settings.logFile = {};
  settings.logLevel = LogLevel::Error;
  settings.includeUGC = false;
  auto created_root = Root::create(std::move(settings));
  ASSERT_TRUE(created_root);
  auto& root = **created_root;
  auto storage = File::ephemeral_file(root.random());
  ASSERT_TRUE(storage);
  auto world = WorldServer::create(root, Vec2U(2048, 2048), *storage);
  ASSERT_TRUE(world) << world.error().what();
  auto close = make_scope_exit([&] { EXPECT_TRUE((*world)->close()); });
  (*world)->setSpawningEnabled(false);
  auto factory = root.playerFactory();
  ASSERT_TRUE(factory);
  auto made = (*factory)->create();
  ASSERT_TRUE(made);
  auto& player = **made;
  ASSERT_TRUE(player.finalizeCreation());
  auto uuid = player.uuid();
  ASSERT_TRUE(uuid);
  auto context = ClientContext::create(root.logger(), Uuid(root.random()), *uuid, 1, {});
  ASSERT_TRUE(context);
  player.set_client_context(*context);
  Entity& entity = player;
  ASSERT_TRUE(entity.init(**world, 100, EntityMode::Master));
  auto uninit_player = make_scope_exit([&] { EXPECT_TRUE(entity.uninit()); });
  ASSERT_TRUE(player.refresh_humanoid());
  ASSERT_TRUE(player.moveTo(Vec2F(1024, 1024)));
  test(root, player);
}
}

const GameTestRegistration item_force_geometry("ItemTest", "ActiveItemForceRegions", [](GameTest& fixture) {
  with_scripted_item_owner(fixture, R"(
function init()
  local radial = {type="RadialForceRegion", center={2,3}, innerRadius=1, outerRadius=4,
                  targetRadialVelocity=9, controlForce=2}
  local area = {{1,2},{5,2},{1,6}}
  activeItem.setArmAngle(0.7)
  activeItem.setForceRegions({radial})
  activeItem.setItemForceRegions({
    {type="DirectionalForceRegion", polyRegion=area, xTargetVelocity=7, controlForce=2},
    radial,
    {type="GradientForceRegion", polyRegion=area, gradient={{2,3},{4,5}}, baseTargetVelocity=8, baseControlForce=3}
  })
  message.setHandler("clearLocal", function() activeItem.setItemForceRegions(nil) return true end)
end
)", [](Root& root, Player& player) {
    Json config = JsonObject{{"itemName", "forcetest"}, {"rarity", "common"}, {"animation", JsonObject{}},
        {"scripts", JsonArray{"/active-item-test.lua"}}};
    auto item = ActiveItem::create(root, config, "/", JsonObject{});
    ASSERT_TRUE(item);
    auto empty = (*item)->force_regions();
    ASSERT_TRUE(empty);
    EXPECT_TRUE(empty->empty());
    ASSERT_TRUE((*item)->init(player, ToolHand::Primary));
    auto cleanup = make_scope_exit([&] {
      if ((*item)->initialized())
        EXPECT_TRUE((*item)->uninit());
    });
    auto& movement = player.movementController();
    for (Direction facing : {Direction::Right, Direction::Left}) {
      auto state = movement.store_state();
      ASSERT_TRUE(state);
      auto directed = state->set("facingDirection", facing == Direction::Left ? Json("left") : Json("right"));
      ASSERT_TRUE(directed);
      ASSERT_TRUE(movement.load_state(*directed));
      ASSERT_EQ(player.facingDirection(), facing);
      auto offset = player.hand_offset(ToolHand::Primary, facing);
      ASSERT_TRUE(offset);
      auto arm = player.arm_position(ToolHand::Primary, facing, 0.7f, *offset);
      ASSERT_TRUE(arm);
      Vec2F origin = player.position() + *arm;
      auto expected_point = [&](Vec2F point) {
        point = point.rotate(0.7f);
        if (facing == Direction::Left)
          point[0] = -point[0];
        return origin + point;
      };
      auto expected_area = PolyF{{1,2},{5,2},{1,6}};
      expected_area.rotate(0.7f);
      if (facing == Direction::Left)
        expected_area.flip_horizontal();
      expected_area.translate(origin);
      auto regions = (*item)->force_regions();
      ASSERT_TRUE(regions) << regions.error().what();
      ASSERT_EQ(regions->size(), 4u);
      ASSERT_TRUE(std::holds_alternative<RadialForceRegion>((*regions)[0]));
      EXPECT_EQ(std::get<RadialForceRegion>((*regions)[0]).center, Vec2F(2,3));
      ASSERT_TRUE(std::holds_alternative<DirectionalForceRegion>((*regions)[1]));
      auto const& directional = std::get<DirectionalForceRegion>((*regions)[1]);
      EXPECT_EQ(directional.region, expected_area);
      EXPECT_EQ(directional.x_target_velocity, 7.f);
      ASSERT_TRUE(std::holds_alternative<RadialForceRegion>((*regions)[2]));
      EXPECT_EQ(std::get<RadialForceRegion>((*regions)[2]).center, expected_point({2,3}));
      ASSERT_TRUE(std::holds_alternative<GradientForceRegion>((*regions)[3]));
      auto const& gradient = std::get<GradientForceRegion>((*regions)[3]);
      EXPECT_EQ(gradient.region, expected_area);
      EXPECT_EQ(gradient.gradient.min(), expected_point({2,3}));
      EXPECT_EQ(gradient.gradient.max(), expected_point({4,5}));
    }
    auto cleared = (*item)->receiveMessage("clearLocal", true, {});
    ASSERT_TRUE(cleared);
    ASSERT_TRUE(*cleared);
    ASSERT_TRUE((*item)->uninit());
    // World-space data remains available without a hand or owner.
    auto detached = (*item)->force_regions();
    ASSERT_TRUE(detached) << detached.error().what();
    ASSERT_EQ(detached->size(), 1u);
    EXPECT_EQ(std::get<RadialForceRegion>(detached->front()).center, Vec2F(2,3));
  });
});

const GameTestRegistration activeItemScriptRecovery(
  "ItemTest", "ActiveItemScriptErrorsStayLocalAndAllowCleanReequip", [](GameTest& fixture) {
    with_scripted_item_owner(fixture, R"(
local function visit(hook)
  activeItem.setInstanceValue("lastHook", hook)
  storage.lastHook = hook
  if root.getConfiguration("recoveryItemFailure") == hook then
    error("active-item-recovery:" .. hook)
  end
end
function init()
  message.setHandler("recoveryProbe", function()
    visit("message")
    return true
  end)
  visit("init")
end
function activate() visit("activate") end
function update() visit("update") end
function uninit() visit("uninit") end
)", [](Root& root, Player& player) {
    class RecoveryLogSink final : public LogSink {
    public:
      std::vector<std::string> records;
      std::expected<void, StarException> log(std::string_view message, LogLevel) override {
        records.emplace_back(message);
        return {};
      }
    };
    auto sink = std::make_shared<RecoveryLogSink>();
    sink->set_level(LogLevel::Error);
    root.logger().add_sink(sink);
    Json config = JsonObject{
      {"itemName", "activeitemrecovery"}, {"rarity", "common"}, {"animation", JsonObject{}},
      {"scripts", JsonArray{"/active-item-test.lua"}}, {"retainScriptStorageInItem", true}
    };
    constexpr float dt = 1.f / 60;
    for (String failure : {String("init"), String("activate"), String("update"), String("message"), String("uninit")}) {
      SCOPED_TRACE(failure.utf8());
      auto configuration = root.configuration();
      ASSERT_TRUE(configuration);
      ASSERT_TRUE((*configuration)->set("recoveryItemFailure", failure));
      auto item = ActiveItem::create(root, config, "/", JsonObject{});
      ASSERT_TRUE(item);
      ASSERT_TRUE((*item)->init(player, ToolHand::Primary));
      auto uninitItem = make_scope_exit([&] {
        if ((*item)->initialized())
          EXPECT_TRUE((*item)->uninit());
      });
      if (failure == "message") {
        auto response = (*item)->receiveMessage("recoveryProbe", true, {});
        ASSERT_TRUE(response);
        EXPECT_FALSE(*response);
      }
      ASSERT_TRUE((*item)->update(dt, FireMode::Primary, false, {}));
      ASSERT_TRUE((*item)->update(dt, FireMode::Primary, false, {}));
      String expectedHook = failure == "uninit" ? "update" : failure;
      EXPECT_EQ((*item)->instance_value("lastHook").and_then([](Json const& value) { return value.to_string(); }), expectedHook);
      // Storage retention is host work after activate/update. A failed script
      // must not prevent it, and the second tick must not re-enter that script.
      auto retained = (*item)->instance_value("scriptStorage");
      ASSERT_TRUE(retained);
      EXPECT_EQ(retained->get_string("lastHook"), expectedHook);
      ASSERT_TRUE((*item)->uninit());
      EXPECT_FALSE((*item)->initialized());
      EXPECT_EQ((*item)->instance_value("lastHook").and_then([](Json const& value) { return value.to_string(); }), failure);
      std::string marker = "active-item-recovery:" + failure.utf8();
      EXPECT_EQ(std::ranges::count_if(sink->records, [&](std::string const& record) {
        return record.find(marker) != std::string::npos;
      }), 1);
      sink->records.clear();

      // Reusing the same item proves uninit removed every callback group even
      // after a failed hook; duplicate registration remains a real error.
      ASSERT_TRUE((*configuration)->set("recoveryItemFailure", "none"));
      ASSERT_TRUE((*item)->init(player, ToolHand::Primary));
      ASSERT_TRUE((*item)->update(dt, FireMode::Primary, false, {}));
      auto response = (*item)->receiveMessage("recoveryProbe", true, {});
      ASSERT_TRUE(response);
      EXPECT_TRUE(*response);
      ASSERT_TRUE((*item)->uninit());
      EXPECT_FALSE((*item)->initialized());
    }

    auto invalid = ActiveItem::create(root, config, "/", JsonObject{{"scriptDelta", "invalid"}});
    ASSERT_TRUE(invalid);
    EXPECT_FALSE((*invalid)->init(player, ToolHand::Primary));
    EXPECT_TRUE((*invalid)->uninit());
    EXPECT_FALSE((*invalid)->initialized());
  });
  });

const GameTestRegistration collection_resolution("CollectionTest", "UnlockRetryLazySourceAndBuilderDirectory", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory);
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto overlay = File::relative_to(*directory, "assets");
  ASSERT_TRUE(File::create_directories(File::relative_to(overlay, "probes")));
  auto put = [&](String const& name, String const& contents) {
    return File::write_file(contents, File::relative_to(overlay, name));
  };
  ASSERT_TRUE(put("probes/entries.collection", R"({"name":"probeitems","type":"item","title":"Items","collectables":{
    "tool":{"item":"collectabletesttool"},"blank":{"item":"collectabletesttool","icon":""},"layers":{"item":"collectabletestlayers"},"broken":{"item":123}}})"));
  auto base_assets = fixture.root().assets();
  ASSERT_TRUE(base_assets);
  auto png = (*base_assets)->bytes("/assetmissing.png");
  ASSERT_TRUE(png);
  ASSERT_TRUE(File::write_file(**png, File::relative_to(overlay, "probes/picture.png")));
  ASSERT_TRUE(put("probes/layers.item", R"({"itemName":"collectabletestlayers","shortdescription":"Layers","rarity":"Common","inventoryIcon":[]})"));
  ASSERT_TRUE(put("probes/a.collection", R"({"name":"duplicateprobe","title":"A","collectables":{"a":{}}})"));
  ASSERT_TRUE(put("probes/b.collection", R"({"name":"duplicateprobe","title":"B","collectables":{"b":{}}})"));
  ASSERT_TRUE(put("probes/tool.item", R"({"itemName":"collectabletesttool","shortdescription":"Tool","rarity":"Common","inventoryIcon":"picture.png","builder":"/probes/build.lua"})"));
  ASSERT_TRUE(put("probes/build.lua", R"(function build(d,c,p)
    root.setConfiguration("collectableFixtureCalls", (root.getConfiguration("collectableFixtureCalls") or 0) + 1)
    return c,p end)"));
  ASSERT_TRUE(put("player.config.patch", R"([{"op":"replace","path":"/collectableUnlock","value":false}])"));
  auto settings = fixture.rootSettings();
  settings.assetSources.push_back(overlay);
  settings.storageDirectory = File::relative_to(*directory, "storage");
  settings.runtimeConfigFile = {}; settings.logDirectory = {}; settings.logFile = {}; settings.includeUGC = false;
  auto created = Root::create(std::move(settings));
  ASSERT_TRUE(created);
  auto& root = **created;
  auto factory = root.playerFactory();
  ASSERT_TRUE(factory);
  auto player = (*factory)->create();
  ASSERT_TRUE(player);
  auto db = root.collection_database();
  ASSERT_TRUE(db);
  auto config = root.configuration();
  ASSERT_TRUE(config);
  ASSERT_TRUE((*config)->set("collectableFixtureCalls", 0));
  auto tool = (*db)->collectable("probeitems", "tool");
  ASSERT_TRUE(tool) << tool.error().what();
  EXPECT_EQ(tool->icon, String("/probes/picture.png"));
  EXPECT_EQ((*config)->get("collectableFixtureCalls"), Json(1));
  auto blank = (*db)->collectable("probeitems", "blank");
  ASSERT_TRUE(blank);
  EXPECT_TRUE(blank->icon.empty());
  auto layers = (*db)->collectable("probeitems", "layers");
  ASSERT_TRUE(layers) << layers.error().what();
  EXPECT_TRUE(layers->icon.empty());
  auto duplicate_group = (*db)->collection("duplicateprobe");
  ASSERT_TRUE(duplicate_group);
  auto surviving = (*db)->collectables("duplicateprobe");
  ASSERT_TRUE(surviving);
  ASSERT_EQ(surviving->size(), 1u);
  EXPECT_EQ((*surviving)[0].name, duplicate_group->title == "A" ? String("a") : String("b"));
  ASSERT_TRUE((*db)->contains("probeitems", "broken"));
  for (unsigned i = 0; i != 2; ++i) {
    auto bad = (*db)->collectable("probeitems", "broken");
    ASSERT_FALSE(bad);
    EXPECT_NE(std::string_view(bad.error().what()).find("/probes/entries.collection"), std::string_view::npos);
  }
  (*player)->pullQueuedMessages();
  EXPECT_FALSE((*player)->add_collectable("probeitems", "missing"));
  EXPECT_FALSE((*player)->add_collectable("probeitems", "tool"));
  EXPECT_FALSE((*player)->log()->has_collectable("probeitems", "tool"));
  EXPECT_TRUE((*player)->pullQueuedMessages().empty());
  ASSERT_TRUE(put("player.config.patch", R"([{"op":"replace","path":"/collectableUnlock","value":"<collectable> in <collection>"}])"));
  auto assets = root.assets();
  ASSERT_TRUE(assets);
  (*assets)->hot_reload();
  auto added = (*player)->add_collectable("probeitems", "tool");
  ASSERT_TRUE(added) << added.error().what();
  EXPECT_TRUE(*added);
  auto duplicate = (*player)->add_collectable("probeitems", "tool");
  ASSERT_TRUE(duplicate);
  EXPECT_FALSE(*duplicate);
  EXPECT_EQ((*player)->pullQueuedMessages(), List<String>({"Tool in Items"}));
});

const GameTestRegistration codex_item_messages(
  "ItemTest", "CodexItemPreservesLearningStateOnHintFailure", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto overlay = File::relative_to(*directory, "assets");
    ASSERT_TRUE(File::create_directory(overlay));
    ASSERT_TRUE(File::write_file(String(R"({
      "id":"codexhintfixture","title":"Codex hint fixture","icon":"/assetmissing.png","contentPages":[]
    })"), File::relative_to(overlay, "hint.codex")));
    auto patch_path = File::relative_to(overlay, "codex.config.patch");
    ASSERT_TRUE(File::write_file(String(R"([
      {"op":"replace","path":"/messages/learned","value":false},
      {"op":"replace","path":"/messages/alreadyKnown","value":false}
    ])"), patch_path));
    auto settings = fixture.rootSettings();
    settings.assetSources.push_back(overlay);
    settings.storageDirectory = File::relative_to(*directory, "storage");
    settings.runtimeConfigFile = {};
    settings.logDirectory = {};
    settings.logFile = {};
    settings.includeUGC = false;
    auto created_root = Root::create(std::move(settings));
    ASSERT_TRUE(created_root);
    auto& root = **created_root;
    auto storage = File::ephemeral_file(root.random());
    ASSERT_TRUE(storage);
    auto world = WorldServer::create(root, Vec2U(2048, 2048), *storage);
    ASSERT_TRUE(world);
    auto close = make_scope_exit([&] { EXPECT_TRUE((*world)->close()); });
    auto factory = root.playerFactory();
    ASSERT_TRUE(factory);
    auto created_player = (*factory)->create();
    ASSERT_TRUE(created_player);
    auto& player = **created_player;
    ASSERT_TRUE(player.finalizeCreation());
    auto uuid = player.uuid();
    ASSERT_TRUE(uuid);
    auto context = ClientContext::create(root.logger(), Uuid(root.random()), *uuid, 1, {});
    ASSERT_TRUE(context);
    player.set_client_context(*context);
    Entity& entity = player;
    ASSERT_TRUE(entity.init(**world, 100, EntityMode::Master));
    auto uninit_player = make_scope_exit([&] { EXPECT_TRUE(entity.uninit()); });
    auto database = root.itemDatabase();
    ASSERT_TRUE(database);
    auto item = inventoryTestItem(**database, "codexhintfixture-codex");
    ASSERT_TRUE(item && *item);
    auto book = as_ref<CodexItem>(**item);
    ASSERT_TRUE(book);
    ASSERT_TRUE(book->get().init(player, ToolHand::Primary));
    auto uninit_item = make_scope_exit([&] { EXPECT_TRUE(book->get().uninit()); });
    auto books = player.codexes();
    String const id = "codexhintfixture";
    ASSERT_FALSE(books->contains(id));
    player.pullQueuedMessages();
    auto initial = books->revision();
    EXPECT_FALSE(book->get().fire_triggered());
    EXPECT_FALSE(books->contains(id));
    EXPECT_EQ(books->revision(), initial);
    EXPECT_TRUE(player.pullQueuedMessages().empty());

    auto assets = root.assets();
    ASSERT_TRUE(assets);
    ASSERT_TRUE(File::write_file(String(R"([
      {"op":"replace","path":"/messages/learned","value":"Learned fixture"},
      {"op":"replace","path":"/messages/alreadyKnown","value":false}
    ])"), patch_path));
    (*assets)->hot_reload();
    ASSERT_TRUE(book->get().fire_triggered());
    EXPECT_TRUE(books->contains(id));
    EXPECT_FALSE(books->is_read(id));
    EXPECT_EQ(player.pullQueuedMessages(), List<String>({"Learned fixture"}));
    ASSERT_TRUE(books->set_read(id, true));
    auto read_revision = books->revision();
    EXPECT_FALSE(book->get().fire_triggered());
    EXPECT_TRUE(books->is_read(id));
    EXPECT_EQ(books->revision(), read_revision);
    EXPECT_TRUE(player.pullQueuedMessages().empty());

    ASSERT_TRUE(File::write_file(String(R"([
      {"op":"replace","path":"/messages/learned","value":"Learned fixture"},
      {"op":"replace","path":"/messages/alreadyKnown","value":"Known fixture"}
    ])"), patch_path));
    (*assets)->hot_reload();
    ASSERT_TRUE(book->get().fire_triggered());
    EXPECT_FALSE(books->is_read(id));
    EXPECT_EQ(player.pullQueuedMessages(), List<String>({"Known fixture"}));
    auto missing_item = inventoryTestItem(**database, "codexhintfixture-codex", 1,
        JsonObject{{"codexId", "missing-codex-hint-fixture"}});
    ASSERT_TRUE(missing_item && *missing_item);
    auto missing = as_ref<CodexItem>(**missing_item);
    ASSERT_TRUE(missing);
    ASSERT_TRUE(missing->get().init(player, ToolHand::Alt));
    auto uninit_missing = make_scope_exit([&] { EXPECT_TRUE(missing->get().uninit()); });
    auto before_missing = books->revision();
    ASSERT_TRUE(missing->get().fire_triggered());
    EXPECT_FALSE(books->contains("missing-codex-hint-fixture"));
    EXPECT_EQ(books->revision(), before_missing);
    EXPECT_TRUE(player.pullQueuedMessages().empty());
  });

const GameTestRegistration blueprintLearning(
  "ItemTest", "BlueprintLearningConstructsOnlyWhenNeededAndPreservesFailureState", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto overlay = File::relative_to(*directory, "assets");
    ASSERT_TRUE(File::create_directories(overlay));
    auto patchPath = File::relative_to(overlay, "player.config.patch");
    ASSERT_TRUE(File::write_file(String(R"([
    {"op":"replace","path":"/blueprintUnlock","value":"Learned: <ItemName>"},
    {"op":"replace","path":"/blueprintAlreadyKnown","value":"Known: <ItemName>"}
  ])"),
                                 patchPath));
    auto settings = fixture.rootSettings();
    settings.assetSources.push_back(overlay);
    settings.storageDirectory = File::relative_to(*directory, "storage");
    settings.runtimeConfigFile = {};
    settings.logDirectory = {};
    settings.logFile = {};
    settings.includeUGC = false;
    auto root = Root::create(std::move(settings));
    ASSERT_TRUE(root) << root.error().what();
    auto factory = (*root)->playerFactory();
    ASSERT_TRUE(factory) << factory.error().what();
    auto created = (*factory)->create();
    ASSERT_TRUE(created) << created.error().what();
    auto& player = **created;
    EXPECT_TRUE(player.pullQueuedMessages().empty());
    auto descriptor = ItemDescriptor::create(
      "perfectlygenericitem", 1, JsonObject{{"shortdescription", "Blueprint Fixture"}, {"blueprintFixture", true}});
    ASSERT_TRUE(descriptor);
    ASSERT_FALSE(player.blueprintKnown(*descriptor));
    auto learned = player.addBlueprint(*descriptor);
    ASSERT_TRUE(learned) << learned.error().what();
    EXPECT_TRUE(*learned);
    EXPECT_TRUE(player.blueprintKnown(*descriptor));
    EXPECT_TRUE(player.blueprints()->isNew(*descriptor));
    EXPECT_EQ(player.pullQueuedMessages(), List<String>({"Learned: Blueprint Fixture"}));

    player.blueprints()->markAsRead(*descriptor);
    auto knownState = player.blueprints()->toJson();
    auto repeated = player.addBlueprint(*descriptor);
    ASSERT_TRUE(repeated) << repeated.error().what();
    EXPECT_FALSE(*repeated);
    EXPECT_TRUE(player.pullQueuedMessages().empty());
    EXPECT_EQ(player.blueprints()->toJson(), knownState);
    auto shown = player.addBlueprint(*descriptor, true);
    ASSERT_TRUE(shown) << shown.error().what();
    EXPECT_FALSE(*shown);
    EXPECT_EQ(player.pullQueuedMessages(), List<String>({"Known: Blueprint Fixture"}));
    EXPECT_EQ(player.blueprints()->toJson(), knownState);

    // A registered item with invalid instance parameters cannot be constructed,
    // including its perfectlygenericitem fallback. Failed new unlocks must not
    // publish either known or unread blueprint state.
    auto database = (*root)->itemDatabase();
    ASSERT_TRUE(database);
    ASSERT_TRUE((*database)->hasItem("perfectlygenericitem"));
    auto broken = ItemDescriptor::create("perfectlygenericitem", 1,
                                         JsonObject{{"maxStack", "not an integer"}, {"blueprintFixture", "broken"}});
    ASSERT_TRUE(broken);
    for (bool showFailure : {false, true}) {
      auto before = player.blueprints()->toJson();
      ASSERT_FALSE(player.blueprintKnown(*broken));
      auto failed = player.addBlueprint(*broken, showFailure);
      ASSERT_FALSE(failed);
      EXPECT_EQ(player.blueprints()->toJson(), before);
      EXPECT_TRUE(player.pullQueuedMessages().empty());
    }

    // A stored blueprint may outlive its item's ability to instantiate. The
    // intentional behavior change is that silently relearning it now succeeds
    // with false rather than surfacing an irrelevant item-factory error.
    player.blueprints()->add(*broken);
    player.blueprints()->markAsRead(*broken);
    auto brokenKnownState = player.blueprints()->toJson();
    auto silentBroken = player.addBlueprint(*broken);
    ASSERT_TRUE(silentBroken) << silentBroken.error().what();
    EXPECT_FALSE(*silentBroken);
    EXPECT_EQ(player.blueprints()->toJson(), brokenKnownState);
    EXPECT_TRUE(player.pullQueuedMessages().empty());
    auto shownBroken = player.addBlueprint(*broken, true);
    EXPECT_FALSE(shownBroken);
    EXPECT_EQ(player.blueprints()->toJson(), brokenKnownState);
    EXPECT_TRUE(player.pullQueuedMessages().empty());

    // Invalid notification values are real asset errors, not a counter around
    // json(). The silent known path must not read either notification resource.
    ASSERT_TRUE(File::write_file(String(R"([
    {"op":"replace","path":"/blueprintUnlock","value":false},
    {"op":"replace","path":"/blueprintAlreadyKnown","value":false}
  ])"),
                                 patchPath));
    auto assets = (*root)->assets();
    ASSERT_TRUE(assets);
    (*assets)->hot_reload();
    auto silent = player.addBlueprint(*descriptor);
    ASSERT_TRUE(silent) << silent.error().what();
    EXPECT_FALSE(*silent);
    EXPECT_TRUE(player.pullQueuedMessages().empty());
    EXPECT_EQ(player.blueprints()->toJson(), brokenKnownState);
    EXPECT_FALSE(player.addBlueprint(*descriptor, true));
    EXPECT_TRUE(player.pullQueuedMessages().empty());
    EXPECT_EQ(player.blueprints()->toJson(), brokenKnownState);
    auto nullBlueprint = player.addBlueprint(ItemDescriptor(), true);
    ASSERT_TRUE(nullBlueprint);
    EXPECT_FALSE(*nullBlueprint);
    EXPECT_EQ(player.blueprints()->toJson(), brokenKnownState);
  });

const GameTestRegistration
  itemScriptLifecycle("ItemTest", "AugmentAndAgingSettleScriptLifecycleBeforeCommittingItems", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto overlay = File::relative_to(*directory, "assets");
    ASSERT_TRUE(File::create_directory(overlay));
    ASSERT_TRUE(File::write_file(String(R"({
      "itemName":"lifecycletarget","inventoryIcon":"/assetmissing.png",
      "shortdescription":"Lifecycle target","description":"Script lifecycle target",
      "rarity":"common","maxStack":1000,"itemAgingScripts":["lifecycle.lua"]
    })"),
                                 File::relative_to(overlay, "target.item")));
    ASSERT_TRUE(File::write_file(String(R"(
local function record(stage)
  local key = "lifecycle" .. stage
  root.setConfiguration(key, root.getConfiguration(key) + 1)
end
function init()
  record("Init")
  local options = root.getConfiguration("lifecycleOptions")
  if item then
    assert(item.name() == "lifecycleaugment")
    assert(config.getParameter("lifecycleCallback") == "installed")
  end
  if options.failStage == "init" then error("item-init-marker") end
end
local function run(descriptor)
  record("Invoke")
  local options = root.getConfiguration("lifecycleOptions")
  if options.failStage == "invoke" then error("item-invoke-marker") end
  if options.malformed then return {name = false} end
  if not options.unchanged then descriptor.parameters.lifecycleChanged = true end
  return descriptor
end
function apply(descriptor)
  local changed = run(descriptor)
  if root.getConfiguration("lifecycleOptions").unchanged then return nil end
  return changed, 2
end
function ageItem(descriptor, aging)
  assert(aging == 7.25)
  return run(descriptor)
end
function uninit()
  record("Cleanup")
  if root.getConfiguration("lifecycleOptions").failCleanup then error("item-cleanup-marker") end
end
    )"),
                                 File::relative_to(overlay, "lifecycle.lua")));
    auto settings = fixture.rootSettings();
    settings.assetSources.push_back(overlay);
    settings.storageDirectory = File::relative_to(*directory, "storage");
    settings.runtimeConfigFile = {};
    settings.logDirectory = {};
    settings.logFile = {};
    settings.includeUGC = false;
    auto root = Root::create(std::move(settings));
    ASSERT_TRUE(root) << root.error().what();
    auto database = (*root)->itemDatabase();
    ASSERT_TRUE(database) << database.error().what();
    auto configuration = (*root)->configuration();
    ASSERT_TRUE(configuration);
    struct Case {
      std::string_view stage;
      bool cleanup;
      bool malformed;
      bool unchanged;
    };
    Case const cases[]{{"", false, false, false}, {"init", false, false, false}, {"invoke", false, false, false},
                       {"", true, false, false},  {"init", true, false, false},  {"invoke", true, false, false},
                       {"", false, true, false},  {"", false, false, true}};
    for (bool aging : {false, true}) {
      SCOPED_TRACE(aging ? "aging" : "augment");
      for (auto const& test : cases) {
        SCOPED_TRACE(test.stage);
        SCOPED_TRACE(test.cleanup);
        SCOPED_TRACE(test.malformed);
        SCOPED_TRACE(test.unchanged);
        ASSERT_TRUE((*configuration)
                      ->set("lifecycleOptions",
                            JsonObject{{"failStage", test.stage},
                                       {"failCleanup", test.cleanup},
                                       {"malformed", test.malformed},
                                       {"unchanged", test.unchanged}}));
        for (std::string_view key : {"lifecycleInit", "lifecycleInvoke", "lifecycleCleanup"})
          ASSERT_TRUE((*configuration)->set(key, 0));
        auto target = inventoryTestItem(**database, "lifecycletarget", 4, JsonObject{{"original", true}});
        ASSERT_TRUE(target) << target.error().what();
        ASSERT_TRUE(*target);
        auto original = *target;
        auto originalDescriptor = original->descriptor();
        bool scriptFailed = !test.stage.empty() || test.cleanup;
        if (aging) {
          auto result = (*database)->ageItem(*target, 7.25);
          EXPECT_EQ(result.has_value(), !scriptFailed && !test.malformed);
          if (result)
            EXPECT_EQ(*result, !test.unchanged);
          else if (scriptFailed)
            EXPECT_TRUE(std::string_view(result.error().what())
                          .contains(test.stage == "init"       ? "item-init-marker"
                                      : test.stage == "invoke" ? "item-invoke-marker"
                                                               : "item-cleanup-marker"));
        } else {
          auto augment = AugmentItem::create(**root,
                                             JsonObject{{"itemName", "lifecycleaugment"},
                                                        {"inventoryIcon", "/assetmissing.png"},
                                                        {"shortdescription", "Lifecycle augment"},
                                                        {"rarity", "common"},
                                                        {"maxStack", 100},
                                                        {"scripts", JsonArray{"lifecycle.lua"}},
                                                        {"lifecycleCallback", "installed"}},
                                             "/", JsonObject());
          ASSERT_TRUE(augment) << augment.error().what();
          ASSERT_EQ((*augment)->set_count(3), 0u);
          auto result = (*database)->applyAugment(*target, **augment);
          EXPECT_EQ(result.has_value(), !scriptFailed && !test.malformed);
          if (result)
            *target = *result;
          else if (scriptFailed)
            EXPECT_TRUE(std::string_view(result.error().what())
                          .contains(test.stage == "init"       ? "item-init-marker"
                                      : test.stage == "invoke" ? "item-invoke-marker"
                                                               : "item-cleanup-marker"));
          // Even a malformed returned descriptor consumes the requested augment
          // count, but no Lua lifecycle failure can reach that consumption.
          EXPECT_EQ((*augment)->count(), !scriptFailed && !test.unchanged ? 1u : 3u);
        }
        bool changed = !scriptFailed && !test.malformed && !test.unchanged;
        ASSERT_TRUE(*target);
        EXPECT_EQ(*target == original, !changed);
        EXPECT_EQ(original->descriptor(), originalDescriptor);
        EXPECT_EQ((*target)->count(), 4u);
        EXPECT_EQ((*target)->parameters().get_bool("lifecycleChanged", false), changed);
        auto initCount = (*configuration)->get("lifecycleInit");
        auto invokeCount = (*configuration)->get("lifecycleInvoke");
        auto cleanupCount = (*configuration)->get("lifecycleCleanup");
        ASSERT_TRUE(initCount);
        ASSERT_TRUE(invokeCount);
        ASSERT_TRUE(cleanupCount);
        EXPECT_EQ(initCount->to_int(), 1);
        EXPECT_EQ(invokeCount->to_int(), test.stage == "init" ? 0 : 1);
        // DisableContext retires failed init/invoke sessions before explicit
        // uninit. A configured second hook failure must not resurrect the hook.
        EXPECT_EQ(cleanupCount->to_int(), test.stage.empty() ? 1 : 0);
      }
    }
  });

const GameTestRegistration
  objectConfigDomains("ItemTest", "ObjectConfigPreservesDamageLightingPresenceAndErrorOrder", [](GameTest& fixture) {
    auto originalDatabase = fixture.root().objectDatabase();
    ASSERT_TRUE(originalDatabase);
    auto original = (*originalDatabase)->getConfig("perfectlygenericitem");
    ASSERT_TRUE(original) << original.error().what();
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto overlay = File::relative_to(*directory, "assets");
    auto relativeDirectory = AssetPath::directory((*original)->path).substr(1);
    ASSERT_TRUE(relativeDirectory);
    auto objectDirectory = File::relative_to(overlay, *relativeDirectory);
    ASSERT_TRUE(File::create_directories(objectDirectory));
    Json base = (*original)->config;
    for (std::string_view key : {"damageTeam", "lightColor", "lightColors", "lightType", "pointLight", "pointBeam",
                                 "beamAmbience", "flickerPeriod", "flickerMinIntensity", "flickerMaxIntensity",
                                 "flickerPeriodVariance", "flickerIntensityVariance"}) {
      auto erased = base.erase(key);
      ASSERT_TRUE(erased);
      base = std::move(*erased);
    }
    struct Case {
      std::string_view name;
      JsonObject fields;
      std::string_view firstError;
      std::string_view laterError;
    };
    Case const cases[]{
      {"domainmissing", {}, "", ""},
      {"domaindefaults",
       {{"damageTeam", JsonObject{}},
        {"lightType", Json()},
        {"pointLight", true},
        {"flickerMinIntensity", "ignored without flickerPeriod"}},
       "",
       ""},
      // Avoid the asset patch's non-point normalization while testing explicit type precedence.
      {"domainlighting",
       {{"damageTeam", JsonObject{{"type", "environment"}, {"team", 17}}},
        {"lightColor", JsonArray{10, 20, 30}},
        {"lightColors", false},
        {"lightType", "Spread"},
        {"pointLight", true},
        {"pointBeam", 0.75},
        {"beamAmbience", 0.25},
        {"flickerPeriod", 2.0},
        {"flickerMinIntensity", 0.3},
        {"flickerMaxIntensity", 0.9},
        {"flickerPeriodVariance", 0.1},
        {"flickerIntensityVariance", 0.2}},
       "",
       ""},
      {"domaincolors", {{"lightColors", JsonObject{{"warm", JsonArray{40, 50, 60}}}}}, "", ""},
      {"domainteamfirst",
       {{"damageTeam", JsonObject{{"type", "unknown-team-marker"}, {"team", false}}},
        {"lightType", "later-light-marker"}},
       "unknown-team-marker",
       "later-light-marker"},
      {"domainlightfirst",
       {{"lightType", "unknown-light-marker"}, {"pointLight", false}, {"pointBeam", false}},
       "unknown-light-marker",
       ""},
      {"domainbeamfirst", {{"pointBeam", false}, {"beamAmbience", "later numeric failure"}}, "bool", "string"},
      {"domainflickerfirst",
       {{"flickerPeriod", false}, {"flickerMinIntensity", "later numeric failure"}},
       "bool",
       "string"},
      {"domainnullteam", {{"damageTeam", Json()}}, "null", ""},
      {"domainnullcolor",
       {{"lightColor", Json()}, {"lightColors", JsonObject{{"warm", JsonArray{1, 2, 3}}}}},
       "null",
       ""}};
    for (auto const& test : cases) {
      auto config =
        base.set_all(test.fields).and_then([&](Json const& value) { return value.set("objectName", test.name); });
      ASSERT_TRUE(config);
      auto contents = config->repr();
      ASSERT_TRUE(contents);
      ASSERT_TRUE(File::write_file(*contents, File::relative_to(objectDirectory, String(test.name) + ".object")));
    }
    auto settings = fixture.rootSettings();
    settings.assetSources.push_back(overlay);
    settings.storageDirectory = File::relative_to(*directory, "storage");
    settings.runtimeConfigFile = {};
    settings.logDirectory = {};
    settings.logFile = {};
    settings.includeUGC = false;
    auto root = Root::create(std::move(settings));
    ASSERT_TRUE(root) << root.error().what();
    auto database = (*root)->objectDatabase();
    ASSERT_TRUE(database) << database.error().what();
    for (auto const& test : cases) {
      SCOPED_TRACE(test.name);
      auto config = (*database)->getConfig(test.name);
      if (!test.firstError.empty()) {
        ASSERT_FALSE(config);
        EXPECT_TRUE(config.error().is<ObjectException>());
        EXPECT_TRUE(std::string_view(config.error().what()).contains(test.firstError));
        if (!test.laterError.empty())
          EXPECT_FALSE(std::string_view(config.error().what()).contains(test.laterError));
        continue;
      }
      ASSERT_TRUE(config) << config.error().what();
      EXPECT_EQ((*config)->name, test.name);
      EXPECT_EQ((*config)->race, (*original)->race);
      EXPECT_EQ((*config)->category, (*original)->category);
      EXPECT_EQ((*config)->orientations.size(), (*original)->orientations.size());
      EXPECT_EQ((*config)->rooting, (*original)->rooting);
      EXPECT_EQ((*config)->biomePlaced, (*original)->biomePlaced);
      if (test.name == "domaindefaults") {
        EXPECT_EQ((*config)->damageTeam.type, TeamType::Environment);
        EXPECT_EQ((*config)->damageTeam.team, 0u);
        EXPECT_EQ((*config)->lightType, LightType::Point);
        EXPECT_FALSE((*config)->lightFlickering);
      } else if (test.name == "domainlighting") {
        EXPECT_EQ((*config)->damageTeam.type, TeamType::Environment);
        EXPECT_EQ((*config)->damageTeam.team, 17u);
        ASSERT_EQ((*config)->lightColors.size(), 1u);
        auto color = (*config)->lightColors.maybe("default");
        ASSERT_TRUE(color);
        EXPECT_EQ(*color, Color::rgb(10, 20, 30));
        EXPECT_EQ((*config)->lightType, LightType::Spread);
        EXPECT_FLOAT_EQ((*config)->pointBeam, 0.75f);
        EXPECT_FLOAT_EQ((*config)->beamAmbience, 0.25f);
        EXPECT_TRUE((*config)->lightFlickering);
      } else if (test.name == "domaincolors") {
        ASSERT_EQ((*config)->lightColors.size(), 1u);
        auto color = (*config)->lightColors.maybe("warm");
        ASSERT_TRUE(color);
        EXPECT_EQ(*color, Color::rgb(40, 50, 60));
      } else {
        EXPECT_TRUE((*config)->lightColors.empty());
        EXPECT_EQ((*config)->lightType, LightType::Spread);
        EXPECT_FLOAT_EQ((*config)->pointBeam, 0.0f);
        EXPECT_FLOAT_EQ((*config)->beamAmbience, 0.0f);
        EXPECT_FALSE((*config)->lightFlickering);
      }
    }
  });
