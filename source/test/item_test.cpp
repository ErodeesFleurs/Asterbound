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
import star.game_types;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import star.version;
import std;

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

namespace Star {

void PrintTo(ItemDescriptor const& descriptor, std::ostream* stream) {
  auto printed = *stream << descriptor;
  if (!printed)
    *stream << "<ItemDescriptor rendering failed: " << printed.error().what() << ">";
}

}

using namespace Star;

namespace {

[[nodiscard]] std::expected<List<ItemDescriptor>, StarException> itemTestDescriptors(List<Json> const& values) {
  List<ItemDescriptor> descriptors;
  for (auto const& value : values) {
    auto descriptor = ItemDescriptor::create(value);
    if (!descriptor)
      return std::unexpected(std::move(descriptor.error()));
    descriptors.append(std::move(*descriptor));
  }
  return descriptors;
}

[[nodiscard]] std::expected<std::shared_ptr<Item>, StarException> inventoryTestItem(
    ItemDatabase const& database, String const& name, std::uint64_t count = 1, Json parameters = Json()) {
  auto descriptor = ItemDescriptor::create(name, count, std::move(parameters));
  if (!descriptor)
    return std::unexpected(std::move(descriptor.error()));
  return database.item(*descriptor);
}

}

const GameTestRegistration itemDescriptorConstruction("ItemTest", "ItemDescriptorConstruction", [](GameTest&) {
  ItemDescriptor testItemDescriptor;

  testItemDescriptor = ItemDescriptor();

  auto nullDescriptor = ItemDescriptor::create(Json());
  ASSERT_TRUE(nullDescriptor);
  testItemDescriptor = std::move(*nullDescriptor);

  String nameOnly = "perfectlygenericitem";
  auto namedDescriptor = ItemDescriptor::create(nameOnly, 1);
  ASSERT_TRUE(namedDescriptor);
  testItemDescriptor = std::move(*namedDescriptor);

  List<JsonArray> arrayFormats = List<JsonArray>{
      JsonArray{"perfectlygenericitem"},
      JsonArray{"perfectlygenericitem", 1},
      JsonArray{"perfectlygenericitem", 1, JsonObject()},
      JsonArray{"perfectlygenericitem", 1, JsonObject{{"testParameter", "testValue"}}}
    };

  for (auto const& arrayFormat : arrayFormats) {
    auto descriptor = ItemDescriptor::create(arrayFormat);
    ASSERT_TRUE(descriptor);
    testItemDescriptor = std::move(*descriptor);
  }

  List<JsonObject> objectFormats = List<JsonObject>{
      JsonObject{{"name", "perfectlygenericitem"}},
      JsonObject{{"item", "perfectlygenericitem"}},
      JsonObject{{"name", "perfectlygenericitem"}, {"count", 1}},
      JsonObject{{"name", "perfectlygenericitem"}, {"count", 1}, {"parameters", JsonObject()}},
      JsonObject{{"name", "perfectlygenericitem"}, {"count", 1}, {"parameters", JsonObject{{"testParameter", "testValue"}}}}
    };

  for (auto const& objectFormat : objectFormats) {
    auto descriptor = ItemDescriptor::create(objectFormat);
    ASSERT_TRUE(descriptor);
    testItemDescriptor = std::move(*descriptor);
  }

  auto countedDescriptor = ItemDescriptor::create("perfectlygenericitem", 1);
  ASSERT_TRUE(countedDescriptor);
  testItemDescriptor = std::move(*countedDescriptor);
  auto parameterizedDescriptor = ItemDescriptor::create("perfectlygenericitem", 1, JsonObject{{"testParameter", "testValue"}});
  ASSERT_TRUE(parameterizedDescriptor);
  testItemDescriptor = std::move(*parameterizedDescriptor);
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

  auto descriptors = itemTestDescriptors({
    JsonArray{"perfectlygenericitem"},
    JsonArray{"perfectlygenericitem", 1},
    JsonArray{"perfectlygenericitem", 1, JsonObject()},
    JsonObject{{"name", "perfectlygenericitem"}},
    JsonObject{{"item", "perfectlygenericitem"}},
    JsonObject{{"name", "perfectlygenericitem"}, {"count", 1}},
    JsonObject{{"name", "perfectlygenericitem"}, {"count", 1}, {"parameters", JsonObject()}}
  });
  ASSERT_TRUE(descriptors);
  auto namedComparison = ItemDescriptor::create("perfectlygenericitem", 1);
  ASSERT_TRUE(namedComparison);
  descriptors->insertAt(0, std::move(*namedComparison));
  auto const& testItemDescriptors = *descriptors;
  auto parameterizedDescriptors = itemTestDescriptors({
    JsonArray{"perfectlygenericitem", 1, JsonObject{{"testParameter", "testValue"}}},
    JsonObject{{"name", "perfectlygenericitem"}, {"count", 1}, {"parameters", JsonObject{{"testParameter", "testValue"}}}}
  });
  ASSERT_TRUE(parameterizedDescriptors);
  auto const& testItemDescriptorsParams = *parameterizedDescriptors;

  // Creating an item can add parameters to it: the perfectly generic item is an
  // object item, and objects that retain their placement parameters start with
  // an empty 'scriptStorage' entry so that they stack correctly.  Exact
  // comparisons therefore work on the parameters the created items carry.
  ItemDescriptor testItemExact = testItem->descriptor().singular();
  ItemDescriptor testItemParamsExact = testItemParams->descriptor().singular();
  EXPECT_EQ(testItemExact.parameters(), JsonObject({{"scriptStorage", JsonObject()}}));
  EXPECT_EQ(testItemParamsExact.parameters(),
      JsonObject({{"testParameter", "testValue"}, {"scriptStorage", JsonObject()}}));

  // The same descriptor spellings again, carrying the items' own parameters.
  auto exactDescriptors = itemTestDescriptors({
    JsonArray{"perfectlygenericitem", 1, testItemExact.parameters()},
    JsonObject{{"name", "perfectlygenericitem"}, {"count", 1}, {"parameters", testItemExact.parameters()}}
  });
  ASSERT_TRUE(exactDescriptors);
  exactDescriptors->insertAt(0, testItemExact);
  auto const& testItemExactDescriptors = *exactDescriptors;
  auto exactParameterizedDescriptors = itemTestDescriptors({
    JsonArray{"perfectlygenericitem", 1, testItemParamsExact.parameters()},
    JsonObject{{"name", "perfectlygenericitem"}, {"count", 1}, {"parameters", testItemParamsExact.parameters()}}
  });
  ASSERT_TRUE(exactParameterizedDescriptors);
  exactParameterizedDescriptors->insertAt(0, testItemParamsExact);
  auto const& testItemParamsExactDescriptors = *exactParameterizedDescriptors;

  // comparisons WITHOUT exactMatch
  for (ItemDescriptor const& id : testItemDescriptors) {
    EXPECT_TRUE(testItem->matches(id));
    EXPECT_TRUE(testItemParams->matches(id));
    EXPECT_TRUE(id.matches(*testItem));
    EXPECT_TRUE(id.matches(*testItemParams));
    for (ItemDescriptor const& id2 : testItemDescriptors)
      EXPECT_TRUE(id.matches(id2));
    for (ItemDescriptor const& id2 : testItemDescriptorsParams)
      EXPECT_TRUE(id.matches(id2));
  }
  for (ItemDescriptor const& id : testItemDescriptorsParams) {
    EXPECT_TRUE(testItem->matches(id));
    EXPECT_TRUE(testItemParams->matches(id));
    EXPECT_TRUE(id.matches(*testItem));
    EXPECT_TRUE(id.matches(*testItemParams));
    for (ItemDescriptor const& id2 : testItemDescriptors)
      EXPECT_TRUE(id.matches(id2));
    for (ItemDescriptor const& id2 : testItemDescriptorsParams)
      EXPECT_TRUE(id.matches(id2));
  }
  EXPECT_TRUE(testItem->matches(*testItemParams));
  EXPECT_TRUE(testItemParams->matches(*testItem));

  // comparisons WITH exactMatch: the parameterless descriptors match neither
  // item, and the descriptors carrying an item's own parameters match exactly.
  for (ItemDescriptor const& id : testItemDescriptors) {
    EXPECT_FALSE(testItem->matches(id, true));
    EXPECT_FALSE(testItemParams->matches(id, true));
    EXPECT_FALSE(id.matches(*testItem, true));
    EXPECT_FALSE(id.matches(*testItemParams, true));
    for (ItemDescriptor const& id2 : testItemDescriptors)
      EXPECT_TRUE(id.matches(id2, true));
    for (ItemDescriptor const& id2 : testItemExactDescriptors)
      EXPECT_FALSE(id.matches(id2, true));
    for (ItemDescriptor const& id2 : testItemParamsExactDescriptors)
      EXPECT_FALSE(id.matches(id2, true));
  }
  for (ItemDescriptor const& id : testItemExactDescriptors) {
    EXPECT_TRUE(testItem->matches(id, true));
    EXPECT_FALSE(testItemParams->matches(id, true));
    EXPECT_TRUE(id.matches(*testItem, true));
    EXPECT_FALSE(id.matches(*testItemParams, true));
    for (ItemDescriptor const& id2 : testItemDescriptors)
      EXPECT_FALSE(id.matches(id2, true));
    for (ItemDescriptor const& id2 : testItemExactDescriptors)
      EXPECT_TRUE(id.matches(id2, true));
    for (ItemDescriptor const& id2 : testItemParamsExactDescriptors)
      EXPECT_FALSE(id.matches(id2, true));
  }
  for (ItemDescriptor const& id : testItemParamsExactDescriptors) {
    EXPECT_FALSE(testItem->matches(id, true));
    EXPECT_TRUE(testItemParams->matches(id, true));
    EXPECT_FALSE(id.matches(*testItem, true));
    EXPECT_TRUE(id.matches(*testItemParams, true));
    for (ItemDescriptor const& id2 : testItemDescriptors)
      EXPECT_FALSE(id.matches(id2, true));
    for (ItemDescriptor const& id2 : testItemExactDescriptors)
      EXPECT_FALSE(id.matches(id2, true));
    for (ItemDescriptor const& id2 : testItemParamsExactDescriptors)
      EXPECT_TRUE(id.matches(id2, true));
  }
  EXPECT_FALSE(testItem->matches(*testItemParams, true));
  EXPECT_FALSE(testItemParams->matches(*testItem, true));
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
    owners.append(entry.second.use_count());
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

const GameTestRegistration nestedReadyCatalog("ItemTest", "BuildersAndRebuildersKeepReadyCatalogAcrossNestedServices", [](GameTest& fixture) {
  auto temporaryDirectory = File::temporaryDirectory(fixture.root().random());
  ASSERT_TRUE(temporaryDirectory);
  auto directory = std::move(*temporaryDirectory);
  auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(directory)); });
  auto overlay = File::relativeTo(directory, "assets");
  auto writeAsset = [&](String const& path, String const& contents) -> std::expected<void, StarException> {
    auto absolute = path.beginsWith("/");
    if (!absolute)
      return std::unexpected(std::move(absolute.error()));
    String relative = path;
    if (*absolute) {
      auto suffix = path.substr(1);
      if (!suffix)
        return std::unexpected(std::move(suffix.error()));
      relative = std::move(*suffix);
    }
    auto destination = File::relativeTo(overlay, relative);
    auto makeDirectory = File::makeDirectoryRecursive(File::dirName(destination));
    if (!makeDirectory)
      return std::unexpected(std::move(makeDirectory.error()));
    return File::writeFile(contents, destination);
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
    creationScripts->append("/humanoid/opensb/create.lua");
  creationScripts->append("/readycatalog/create.lua");
  auto buildScripts = humanConfig.getArray("buildScripts", {});
  ASSERT_TRUE(buildScripts);
  if (buildScripts->empty())
    buildScripts->append("/humanoid/opensb/build.lua");
  buildScripts->append("/readycatalog/humanoid.lua");
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
  settings.assetSources.append(overlay);
  settings.storageDirectory = File::relativeTo(directory, "storage");
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

const GameTestRegistration blueprintLearning(
  "ItemTest", "BlueprintLearningConstructsOnlyWhenNeededAndPreservesFailureState", [](GameTest& fixture) {
    auto directory = File::temporaryDirectory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(*directory)); });
    auto overlay = File::relativeTo(*directory, "assets");
    ASSERT_TRUE(File::makeDirectoryRecursive(overlay));
    auto patchPath = File::relativeTo(overlay, "player.config.patch");
    ASSERT_TRUE(File::writeFile(String(R"([
    {"op":"replace","path":"/blueprintUnlock","value":"Learned: <ItemName>"},
    {"op":"replace","path":"/blueprintAlreadyKnown","value":"Known: <ItemName>"}
  ])"),
                                patchPath));
    auto settings = fixture.rootSettings();
    settings.assetSources.append(overlay);
    settings.storageDirectory = File::relativeTo(*directory, "storage");
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
    ASSERT_TRUE(File::writeFile(String(R"([
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
