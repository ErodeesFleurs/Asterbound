import star.json;
import star.set;
import star.string;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
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
import star.version;
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
import star.format;
import star.game_test;
import star.algorithm;
import star.file;
import star.random;
import star.collection_database;
import star.player;
import star.player_blueprints;
import star.player_factory;
import star.augment_item;
import star.object_database;
import star.damage_types;
import star.light_source;

import star.beam_item;
import star.casting;
import star.client_context;
import star.entity_rendering;
import star.fireable_item;
import star.game_types;
import star.particle;
import star.tool_user;
import star.uuid;
import star.vector;
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

[[nodiscard]] std::expected<std::shared_ptr<Item>, StarException> inventoryTestItem(
    ItemDatabase const& database, String const& name, std::uint64_t count = 1, Json parameters = Json()) {
  auto descriptor = ItemDescriptor::create(name, count, std::move(parameters));
  if (!descriptor)
    return std::unexpected(std::move(descriptor.error()));
  return database.item(*descriptor);
}

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
      auto cleanup = finally([&] { EXPECT_TRUE(File::remove_all(*directory)); });
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
      sink->setLevel(LogLevel::Error);
      (*root)->logger().addSink(sink);
      auto descriptor = ItemDescriptor::create("missing-item-diagnostics-probe", 37,
          JsonObject{{"scriptStorage", JsonObject{{"savedState", "private-saved-state-marker"}}}});
      ASSERT_TRUE(descriptor);
      auto storedDescriptor = descriptor->toJson().eraseKey("count");
      ASSERT_TRUE(storedDescriptor);
      for (auto level : {LogLevel::Error, LogLevel::Debug}) {
        sink->setLevel(level);
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
  ASSERT_EQ(target->maxStack(), 5u);

  EXPECT_FALSE(target->stackableWith(std::nullopt));
  EXPECT_EQ(target->couldStack(std::nullopt), 0u);
  EXPECT_FALSE(target->stackWith(std::nullopt));
  EXPECT_TRUE(Item::itemsEqual(std::nullopt, std::nullopt));
  EXPECT_FALSE(Item::itemsEqual(std::cref(*target), std::nullopt));
  EXPECT_FALSE(Item::itemsEqual(std::nullopt, std::cref(*target)));
  EXPECT_TRUE(Item::itemsEqual(std::cref(*target), std::cref(*source)));
  EXPECT_TRUE(target->matches(*different));
  EXPECT_FALSE(target->matches(*different, true));
  EXPECT_FALSE(target->stackableWith(std::cref(*different)));
  EXPECT_EQ(target->couldStack(std::cref(*different)), 0u);
  EXPECT_FALSE(target->stackWith(std::ref(*different)));
  EXPECT_EQ(different->count(), 3u);
  EXPECT_EQ(target->count(), 4u);

  EXPECT_EQ(target->couldStack(std::cref(*source)), 1u);
  EXPECT_TRUE(target->stackWith(std::ref(*source)));
  EXPECT_EQ(target->count(), 5u);
  EXPECT_EQ(source->count(), 2u);
  EXPECT_EQ(target->couldStack(std::cref(*source)), 0u);
  EXPECT_FALSE(target->stackWith(std::ref(*source)));
  EXPECT_EQ(source->count(), 2u);

  ASSERT_TRUE(target->consume(3));
  EXPECT_EQ(target->couldStack(std::cref(*source)), 2u);
  EXPECT_TRUE(target->stackWith(std::ref(*source)));
  EXPECT_EQ(target->count(), 4u);
  EXPECT_TRUE(source->empty());
  EXPECT_FALSE(target->stackWith(std::ref(*source)));
  EXPECT_EQ(target->count(), 4u);

  // The source may alias the destination: consuming and adding must balance.
  target->setCount(2);
  EXPECT_TRUE(target->stackWith(std::ref(*target)));
  EXPECT_EQ(target->count(), 2u);
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
  blocker->setCount(1);
  current = std::as_const(bag).at(1);
  ASSERT_TRUE(current);
  EXPECT_EQ(*current, blocker);
  blocker->setCount(0);
  added = bag.addItems(source);
  ASSERT_TRUE(added);
  EXPECT_FALSE(*added);
  current = std::as_const(bag).at(1);
  ASSERT_TRUE(current);
  EXPECT_EQ(*current, source);
  source->setCount(0);
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
  first->setCount(0);
  second->setCount(0);
  auto firstOwners = first.use_count();
  EXPECT_EQ(indexedBag.at(0), nullptr);
  EXPECT_EQ(first.use_count(), firstOwners);
  first->setCount(1);
  EXPECT_EQ(indexedBag.at(0), first);
  first->setCount(0);
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
  emptyItem->setCount(0);
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
  auto object = bags->toObject();
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
  ASSERT_TRUE(inventory.setCustomBarPrimarySlot(0, InventorySlot(bagSlot)));
  inventory.selectActionBarLocation(CustomBarIndex(0));

  List<long> owners;
  for (auto const& entry : items) {
    ASSERT_TRUE(entry.second->consume(1));
    owners.push_back(entry.second.use_count());
    EXPECT_EQ(view.itemsAt(entry.first), nullptr);
  }

  EXPECT_FALSE(view.headArmor());
  EXPECT_FALSE(view.equipment(EquipmentSlot::Head));
  auto bag = view.bagContents(bagSlot.first);
  ASSERT_TRUE(bag);
  EXPECT_EQ(bag->at(bagSlot.second), nullptr);
  EXPECT_FALSE(view.swapSlotItem());
  EXPECT_EQ(view.primaryHeldItem(), nullptr);
  EXPECT_EQ(view.secondaryHeldItem(), nullptr);
  auto primaryHeldSlot = view.primaryHeldSlot();
  ASSERT_TRUE(primaryHeldSlot);
  EXPECT_EQ(*primaryHeldSlot, std::optional<InventorySlot>(InventorySlot(bagSlot)));
  auto all = view.allItems();
  ASSERT_TRUE(all);
  EXPECT_TRUE(all->empty());
  auto summary = view.itemSummary();
  ASSERT_TRUE(summary);
  EXPECT_TRUE(summary->empty());
  auto available = view.availableItems();
  ASSERT_TRUE(available);
  EXPECT_TRUE(available->empty());
  std::size_t visited = 0;
  ASSERT_TRUE(view.forEveryItem([&](InventorySlot const&, std::shared_ptr<Item> const&) -> std::expected<void, StarException> {
    ++visited;
    return {};
  }));
  EXPECT_EQ(visited, 0u);

  // Restoring a shared alias makes the same slots visible again: no getter
  // released the inventory's ownership of the exhausted items.
  for (std::size_t i = 0; i < items.size(); ++i) {
    auto const& entry = items[i];
    EXPECT_EQ(entry.second.use_count(), owners[i]);
    entry.second->setCount(1);
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
  auto customBarPrimarySlot = view.customBarPrimarySlot(0);
  ASSERT_TRUE(customBarPrimarySlot);
  EXPECT_FALSE(*customBarPrimarySlot);
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

  auto missingRead = std::as_const(inventory).itemsAt(BagSlot("missingInventoryTestBag", 0));
  ASSERT_FALSE(missingRead);
  EXPECT_TRUE(missingRead.error().is<ItemException>());
  auto missingTake = inventory.takeSlot(BagSlot("missingInventoryTestBag", 0));
  ASSERT_FALSE(missingTake);
  EXPECT_TRUE(missingTake.error().is<ItemException>());
  auto bag = inventory.bagContents(bagSlot.first);
  ASSERT_TRUE(bag);
  auto bagSize = bag->size();
  ASSERT_LE(bagSize, std::numeric_limits<std::uint8_t>::max());
  BagSlot pastEnd(bagSlot.first, static_cast<std::uint8_t>(bagSize));
  auto pastEndRead = std::as_const(inventory).itemsAt(pastEnd);
  ASSERT_FALSE(pastEndRead);
  EXPECT_TRUE(pastEndRead.error().is<OutOfRangeException>());
  auto pastEndTake = inventory.takeSlot(pastEnd);
  ASSERT_FALSE(pastEndTake);
  EXPECT_TRUE(pastEndTake.error().is<OutOfRangeException>());
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
  ASSERT_TRUE(master.setCustomBarPrimarySlot(0, InventorySlot(bagSlot)));

  auto initial = master.writeNetState();
  ASSERT_TRUE(initial);
  ASSERT_TRUE(slave.readNetState(initial->first));
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
  ASSERT_TRUE(master.forEveryItem([&](InventorySlot const&, std::shared_ptr<Item>& current) -> std::expected<void, StarException> {
    ++visited;
    EXPECT_TRUE(current->consume(current->count()));
    return {};
  }));
  EXPECT_EQ(visited, 2u);
  EXPECT_EQ(item.use_count(), 1);
  auto masterItems = master.allItems();
  ASSERT_TRUE(masterItems);
  EXPECT_TRUE(masterItems->empty());
  auto delta = master.writeNetState(initial->second);
  ASSERT_TRUE(delta);
  ASSERT_TRUE(slave.readNetState(delta->first));
  auto slaveItems = slave.allItems();
  ASSERT_TRUE(slaveItems);
  EXPECT_TRUE(slaveItems->empty());
  auto slaveCustomBarPrimarySlot = slave.customBarPrimarySlot(0);
  ASSERT_TRUE(slaveCustomBarPrimarySlot);
  EXPECT_FALSE(*slaveCustomBarPrimarySlot);

  {
    auto swap = inventoryTestItem(**itemDatabase, "perfectlygenericitem");
    ASSERT_TRUE(swap);
    ASSERT_EQ(master.setItem(SwapSlot(), std::move(*swap)), true);
  }
  auto liveSwap = master.writeNetState(delta->second);
  ASSERT_TRUE(liveSwap);
  ASSERT_TRUE(slave.readNetState(liveSwap->first));
  EXPECT_TRUE(slave.swapSlotItem());
  auto swapAlias = master.swapSlotItem();
  ASSERT_TRUE(swapAlias);
  ASSERT_TRUE(swapAlias->consume(1));
  auto emptySwap = master.writeNetState(liveSwap->second);
  ASSERT_TRUE(emptySwap);
  ASSERT_TRUE(slave.readNetState(emptySwap->first));
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
  ReadyCatalogErrorSink() { setLevel(LogLevel::Error); }
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
    auto cleanup = finally([&] { EXPECT_TRUE(File::remove_all(directory)); });
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
    auto speciesPaths = baseAssets->scanExtension("species");
    ASSERT_TRUE(speciesPaths);
    for (auto const& assetPath : speciesPaths->get()) {
      auto const& path = assetPath.string();
      auto config = baseAssets->json(path);
      ASSERT_TRUE(config);
      auto kind = config->getString("kind");
      ASSERT_TRUE(kind);
      if (*kind == "human") {
        humanPath = path;
        humanConfig = std::move(*config);
        break;
      }
    }
    ASSERT_FALSE(humanPath.empty());
    Json npcConfig;
    auto npcPaths = baseAssets->scanExtension("npctype");
    ASSERT_TRUE(npcPaths);
    for (auto const& assetPath : npcPaths->get()) {
      auto const& path = assetPath.string();
      auto config = baseAssets->json(path);
      ASSERT_TRUE(config);
      auto type = config->getString("type");
      ASSERT_TRUE(type);
      if (*type == "villager") {
        npcConfig = std::move(*config);
        break;
      }
    }
    ASSERT_FALSE(npcConfig.isNull());
    auto creationScripts = humanConfig.getArray("createScripts", {});
    ASSERT_TRUE(creationScripts);
    if (creationScripts->empty())
      creationScripts->push_back("/humanoid/opensb/create.lua");
    creationScripts->push_back("/readycatalog/create.lua");
    auto buildScripts = humanConfig.getArray("buildScripts", {});
    ASSERT_TRUE(buildScripts);
    if (buildScripts->empty())
      buildScripts->push_back("/humanoid/opensb/build.lua");
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
    root.logger().addSink(std::make_shared<ReadyCatalogErrorSink>());

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
    EXPECT_EQ(built->friendlyName(), "NestedReady_LeafOwner");
    EXPECT_EQ(built->parameters().getString("nestedCatalog"), "builder");
    auto rebuiltResult = inventoryTestItem(*database, "readycatalogbroken", 3);
    ASSERT_TRUE(rebuiltResult);
    auto const& rebuilt = *rebuiltResult;
    ASSERT_TRUE(rebuilt);
    EXPECT_EQ(rebuilt->name(), "readycatalogleaf");
    EXPECT_EQ(rebuilt->count(), 3u);
    EXPECT_EQ(rebuilt->friendlyName(), "RebuiltReady_LeafOwner");
    EXPECT_EQ(rebuilt->parameters().getString("nestedCatalog"), "rebuilder");
    auto collections = root.collectionDatabase();
    ASSERT_TRUE(collections);
    auto leaf = (*collections)->collectable("readycatalogcollection", "leaf");
    ASSERT_TRUE(leaf);
    EXPECT_EQ(leaf->title, "LeafOwner");
    auto configuration = root.configuration();
    ASSERT_TRUE(configuration);
    auto marker = (*configuration)->get("readySpeciesBuild");
    ASSERT_TRUE(marker);
    EXPECT_EQ(marker->toString(), "LeafOwner");
  });

const GameTestRegistration
  beamPresentationBoundary("ItemTest", "BeamTransitionsBelongToTicksNotDisplaySampling", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::remove_all(*directory)); });
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
    auto close = finally([&] { EXPECT_TRUE((*world)->close()); });
    (*world)->setSpawningEnabled(false);
    auto factory = root.playerFactory();
    ASSERT_TRUE(factory);
    auto made = (*factory)->create();
    ASSERT_TRUE(made);
    auto& player = **made;
    ASSERT_TRUE(player.finalizeCreation());
    auto uuid = player.uuid();
    ASSERT_TRUE(uuid);
    auto context = ClientContext::create(root, Uuid(root.random()), *uuid);
    ASSERT_TRUE(context);
    player.setClientContext(*context);
    ASSERT_TRUE(player.init(**world, 100, EntityMode::Master));
    auto uninitPlayer = finally([&] { EXPECT_TRUE(player.uninit()); });
    ASSERT_TRUE(player.refreshHumanoid());
    Vec2F const origin(1024, 1024);
    ASSERT_TRUE(player.moveTo(origin));
    auto tools = ToolUser::create(root);
    ASSERT_TRUE(tools);
    ASSERT_TRUE((*tools)->init(player));
    auto uninitTools = finally([&] { EXPECT_TRUE((*tools)->uninit()); });
    auto database = root.itemDatabase();
    ASSERT_TRUE(database);
    auto item = inventoryTestItem(**database, "painttool");
    ASSERT_TRUE(item);
    auto beam = as<BeamItem>(item->get());
    ASSERT_TRUE(beam);
    auto fireable = as<FireableItem>(item->get());
    ASSERT_TRUE(fireable);

    struct Events : NullRenderCallback {
      std::vector<std::tuple<Vec2F, Vec2F, Color>> particles;
      std::expected<void, StarException> addParticle(Particle particle) override {
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
      beam->setRange(8);
      player.aim(origin + Vec2F(2, 0));
      // Equipping deliberately triggers the real item's cooldown. Advance
      // fixed ticks through it before requiring a visible painting beam.
      unsigned equipTicks = (unsigned)std::ceil(fireable->cooldownTime() / dt) + 1;
      for (unsigned i = 0; i != equipTicks; ++i)
        ASSERT_TRUE((*tools)->tick(dt, false, {}));
      auto warm = beam->nonRotatedDrawables();
      ASSERT_TRUE(warm);
      ASSERT_FALSE(warm->empty());
      ASSERT_TRUE(player.collectRenderEvents(discard));

      // This private Root has no UniverseServer/background simulation threads.
      constexpr std::uint64_t seed = 0x4245414d5449434b;
      root.random().init(seed);
      RandomSource untouched(seed);
      for (unsigned i = 0; i != displayCalls; ++i) {
        auto sampled = beam->nonRotatedDrawables();
        ASSERT_TRUE(sampled);
        EXPECT_FALSE(sampled->empty());
      }
      EXPECT_EQ(root.random().randu64(), untouched.randu64());

      // Changing input is not itself a beam tick. Display sampling must neither
      // observe the future transition nor enqueue its leftovers.
      player.aim(origin + Vec2F(20, 0));
      for (unsigned i = 0; i != displayCalls; ++i) {
        auto sampled = beam->nonRotatedDrawables();
        ASSERT_TRUE(sampled);
        EXPECT_FALSE(sampled->empty());
      }
      Events events;
      ASSERT_TRUE(player.collectRenderEvents(events));
      EXPECT_TRUE(events.particles.empty());
      EXPECT_EQ(root.random().randu64(), untouched.randu64());

      ASSERT_TRUE((*tools)->tick(dt, false, {}));
      for (unsigned i = 0; i != displayCalls; ++i) {
        auto sampled = beam->nonRotatedDrawables();
        ASSERT_TRUE(sampled);
        EXPECT_TRUE(sampled->empty());
      }
      ASSERT_TRUE(player.collectRenderEvents(events));
      ASSERT_FALSE(events.particles.empty());
      for (auto const& particle : events.particles) {
        EXPECT_GT(std::get<0>(particle)[0], origin[0] - 8);
        EXPECT_GT(std::get<0>(particle)[1], origin[1] - 8);
      }
      auto count = events.particles.size();
      ASSERT_TRUE(player.collectRenderEvents(events));
      EXPECT_EQ(events.particles.size(), count);
      ASSERT_TRUE((*tools)->tick(dt, false, {}));
      ASSERT_TRUE(player.collectRenderEvents(events));
      EXPECT_EQ(events.particles.size(), count);
      auto nextRandom = root.random().randu64();
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

const GameTestRegistration blueprintLearning(
  "ItemTest", "BlueprintLearningConstructsOnlyWhenNeededAndPreservesFailureState", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::remove_all(*directory)); });
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
    (*assets)->hotReload();
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
    auto cleanup = finally([&] { EXPECT_TRUE(File::remove_all(*directory)); });
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
          ASSERT_EQ((*augment)->setCount(3), 0u);
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
        EXPECT_EQ((*target)->parameters().getBool("lifecycleChanged", false), changed);
        auto initCount = (*configuration)->get("lifecycleInit");
        auto invokeCount = (*configuration)->get("lifecycleInvoke");
        auto cleanupCount = (*configuration)->get("lifecycleCleanup");
        ASSERT_TRUE(initCount);
        ASSERT_TRUE(invokeCount);
        ASSERT_TRUE(cleanupCount);
        EXPECT_EQ(initCount->toInt(), 1);
        EXPECT_EQ(invokeCount->toInt(), test.stage == "init" ? 0 : 1);
        // DisableContext retires failed init/invoke sessions before explicit
        // uninit. A configured second hook failure must not resurrect the hook.
        EXPECT_EQ(cleanupCount->toInt(), test.stage.empty() ? 1 : 0);
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
    auto cleanup = finally([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto overlay = File::relative_to(*directory, "assets");
    auto relativeDirectory = AssetPath::directory((*original)->path).substr(1);
    ASSERT_TRUE(relativeDirectory);
    auto objectDirectory = File::relative_to(overlay, *relativeDirectory);
    ASSERT_TRUE(File::create_directories(objectDirectory));
    Json base = (*original)->config;
    for (std::string_view key : {"damageTeam", "lightColor", "lightColors", "lightType", "pointLight", "pointBeam",
                                 "beamAmbience", "flickerPeriod", "flickerMinIntensity", "flickerMaxIntensity",
                                 "flickerPeriodVariance", "flickerIntensityVariance"}) {
      auto erased = base.eraseKey(key);
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
        base.setAll(test.fields).and_then([&](Json const& value) { return value.set("objectName", test.name); });
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
