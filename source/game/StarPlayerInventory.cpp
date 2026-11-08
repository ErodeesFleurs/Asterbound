import star.json;
import star.json_extra;
import star.data_stream_extra;
import star.id_map;
import star.ordered_map;
import star.lru_cache;
import star.thread;
import star.time;
import star.random;
import star.ttl_cache;
import star.casting;
import star.game_types;
import star.list;
import star.rpc_promise;
import star.optional;
import star.bi_map;
import star.a_star;
import star.data_stream;
import star.strong_typedef;
import star.multi_array;
import star.string;
import star.color;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.set;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.format;
import star.version;
import star.array;
import star.ordered_set;
import star.audio;
import star.map;
import star.either;
import star.periodic_function;

import star.lua_root;

import star.lua_components;
import star.lua_actor_movement_component;
import star.lua_animation_component;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.tile_damage;
import star.interaction_types;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_modification;
import star.force_regions;
import star.world;
import star.physics_entity;
import star.movement_controller;
import star.platformer_astar_types;
import star.anchorable_entity;

import star.game_timers;
import star.actor_movement_controller;
import star.item;
import star.item_descriptor;
import star.item_database;
import star.item_recipe;
import star.inventory_types;
import star.asset_source;
import star.assets;
import star.configuration;
import star.root;
import star.status_effect_item;
import star.effect_source_item;
import star.previewable_item;
import star.animation;
import star.particle;
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.tool_user_item;
import star.fireable_item;
import star.swingable_item;
import star.armors;
import star.drawable;
import star.pointable_item;

import star.player_inventory;

import star.uuid;
import star.animated_part_set;
import star.mixer;
import star.networked_animator;
import star.humanoid;
import star.lounging_entities;
import star.chat_action;
import star.chatty_entity;
import star.emote_entity;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.inspectable_entity;
import star.ai_types;
import star.entity_rendering_types;
import star.entity_rendering;
import star.player_types;
import star.radio_message_database;
import star.player;

import star.currency_item;
import star.material_item;
import star.object_item;
import star.item_bag;


import star.net_element_basic_fields;
import star.net_element;
import star.net_element_sync_group;

import star.net_compatibility;
import star.math_common;
import star.algorithm;
namespace Star {

[[nodiscard]] std::expected<bool, StarException> PlayerInventory::itemAllowedInBag(std::shared_ptr<Item> const& items, String const& bagType) const {
  // any inventory type can have empty slots
  if (!items || items->empty())
    return true;

  return checkInventoryFilter(items, bagType);
}

bool PlayerInventory::itemAllowedAsEquipment(std::shared_ptr<Item> const& item, EquipmentSlot equipmentSlot) {
  // any equipment slot can be empty
  if (!item || item->empty())
    return true;

  if (equipmentSlot == EquipmentSlot::Head || equipmentSlot == EquipmentSlot::HeadCosmetic)
    return is<HeadArmor>(item);
  else if (equipmentSlot == EquipmentSlot::Chest || equipmentSlot == EquipmentSlot::ChestCosmetic)
    return is<ChestArmor>(item);
  else if (equipmentSlot == EquipmentSlot::Legs || equipmentSlot == EquipmentSlot::LegsCosmetic)
    return is<LegsArmor>(item);
  else if (equipmentSlot == EquipmentSlot::Back || equipmentSlot == EquipmentSlot::BackCosmetic)
    return is<BackArmor>(item);
  else
    return is<ArmorItem>(item);
}

PlayerInventory::PlayerInventory(Root& root) : PlayerInventory(root, Json(), Json()) {}

[[nodiscard]] std::expected<std::shared_ptr<PlayerInventory>, StarException> PlayerInventory::create(Root& root) {
  auto assets = root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/player.config:inventory");
  if (!config) return std::unexpected(std::move(config.error()));
  auto currencies = (*assets)->json("/currencies.config");
  if (!currencies) return std::unexpected(std::move(currencies.error()));
  return create(root, std::move(*config), std::move(*currencies));
}

PlayerInventory::PlayerInventory(Root& root, Json inventoryConfig, Json currenciesConfig)
  : m_root(root), m_inventoryConfig(std::move(inventoryConfig)), m_currenciesConfig(std::move(currenciesConfig)),
    m_customBarGroup(0), m_equipmentVisibilityMask(0xFFFFFFFF) {}

[[nodiscard]] std::expected<std::shared_ptr<PlayerInventory>, StarException> PlayerInventory::create(Root& root, Json inventoryConfig, Json currenciesConfig) {
  auto inventory = std::shared_ptr<PlayerInventory>(new PlayerInventory(root, std::move(inventoryConfig), std::move(currenciesConfig)));
  auto initialized = inventory->initialize();
  if (!initialized) return std::unexpected(std::move(initialized.error()));
  return inventory;
}

[[nodiscard]] std::expected<void, StarException> PlayerInventory::initialize() {
  if (m_inventoryConfig.isNull()) {
    auto assets = m_root.assets();
    if (!assets) return std::unexpected(std::move(assets.error()));
    auto config = (*assets)->json("/player.config:inventory");
    if (!config) return std::unexpected(std::move(config.error()));
    auto currencies = (*assets)->json("/currencies.config");
    if (!currencies) return std::unexpected(std::move(currencies.error()));
    m_inventoryConfig = std::move(*config);
    m_currenciesConfig = std::move(*currencies);
  }
  auto bags = m_inventoryConfig.getObject("itemBags");
  if (!bags) return std::unexpected(std::move(bags.error()));
  List<std::pair<String, std::int64_t>> bagOrder;
  for (auto const& bag : *bags) {
    auto priority = bag.second.getInt("priority", 0);
    if (!priority) return std::unexpected(std::move(priority.error()));
    bagOrder.push_back({bag.first, *priority});
  }
  bagOrder.sort([](auto const& a, auto const& b) { return a.second < b.second; });
  for (auto const& entry : bagOrder) {
    auto config = bags->get(entry.first);
    if (!config) return std::unexpected(std::move(config.error()));
    auto size = config->get().getUInt("size");
    if (!size) return std::unexpected(std::move(size.error()));
    m_bags[entry.first] = std::make_shared<ItemBag>(m_root, *size);
    m_bagsNetState[entry.first].resize(*size);
  }
  auto currencies = m_currenciesConfig.toObject();
  if (!currencies) return std::unexpected(std::move(currencies.error()));
  for (auto const& currency : *currencies) m_currencies[currency.first] = 0;
  auto groups = m_inventoryConfig.getUInt("customBarGroups");
  if (!groups) return std::unexpected(std::move(groups.error()));
  auto indexes = m_inventoryConfig.getUInt("customBarIndexes");
  if (!indexes) return std::unexpected(std::move(indexes.error()));
  std::size_t customBarGroups = *groups;
  std::size_t customBarIndexes = *indexes;
  m_customBar.resize(customBarGroups, customBarIndexes);

  if (!EquipmentSlotNames) return std::unexpected(EquipmentSlotNames.error());
  for (auto slot : *EquipmentSlotNames) {
    auto& element = m_equipmentNetState[slot.first];
    if (slot.first > EquipmentSlot::BackCosmetic)
      element.setCompatibilityVersion(9);
    auto added = addNetElement(element);
    if (!added) return std::unexpected(std::move(added.error()));
  }

  for (auto& bag : m_bagsNetState) {
    for (auto& element : bag.second) {
      auto added = addNetElement(element);
      if (!added) return std::unexpected(std::move(added.error()));
    }
  }
  for (std::reference_wrapper<NetElement> element : {std::ref<NetElement>(m_swapSlotNetState), std::ref<NetElement>(m_trashSlotNetState),
      std::ref<NetElement>(m_currenciesNetState), std::ref<NetElement>(m_customBarGroupNetState)}) {
    auto added = addNetElement(element.get());
    if (!added) return std::unexpected(std::move(added.error()));
  }
  m_customBarNetState.resize(customBarGroups, customBarIndexes);
  for (std::size_t i = 0; i < customBarGroups; ++i) {
    for (std::size_t j = 0; j < customBarIndexes; ++j) {
      auto added = addNetElement(m_customBarNetState(i, j));
      if (!added) return std::unexpected(std::move(added.error()));
    }
  }
  auto selected = addNetElement(m_selectedActionBarNetState);
  if (!selected) return std::unexpected(std::move(selected.error()));
  for (auto item : {EssentialItem::BeamAxe, EssentialItem::WireTool, EssentialItem::PaintTool, EssentialItem::InspectionTool}) {
    auto added = addNetElement(m_essentialNetState[item]);
    if (!added) return std::unexpected(std::move(added.error()));
  }

  return {};
}

[[nodiscard]] std::expected<std::shared_ptr<Item>, StarException> PlayerInventory::itemsAt(InventorySlot const& slot) const {
  return retrieve(slot);
}

[[nodiscard]] std::expected<std::shared_ptr<Item>, StarException> PlayerInventory::stackWith(InventorySlot const& slot, std::shared_ptr<Item> const& items) {
  if (!items || items->empty())
    return {};

  auto destination = retrieveForMutation(slot);
  if (!destination) return std::unexpected(std::move(destination.error()));
  auto& dest = destination->get();
  if (std::holds_alternative<EquipmentSlot>(slot)) {
    if (!dest && itemAllowedAsEquipment(items, std::get<EquipmentSlot>(slot))) {
      auto taken = items->take(1);
      if (!taken) return std::unexpected(std::move(taken.error()));
      dest = std::move(*taken);
    }
  } else {
    if (dest && dest->stackableWith(optionalRef(items))) dest->stackWith(optionalRef(items));
    if (!dest) {
      auto taken = items->take(items->count());
      if (!taken) return std::unexpected(std::move(taken.error()));
      dest = std::move(*taken);
    }
    normalizeItem(dest);
  }

  if (items->empty())
    return {};

  return items;
}

[[nodiscard]] std::expected<std::shared_ptr<Item>, StarException> PlayerInventory::takeSlot(InventorySlot const& slot) {
  if (std::holds_alternative<SwapSlot>(slot))
    m_swapReturnSlot = {};
  auto item = retrieveForMutation(slot);
  if (!item) return std::unexpected(std::move(item.error()));
  return std::exchange(item->get(), {});
}

[[nodiscard]] std::expected<bool, StarException> PlayerInventory::exchangeItems(InventorySlot const& first, InventorySlot const& second) {
  auto firstResult = retrieveForMutation(first);
  if (!firstResult) return std::unexpected(std::move(firstResult.error()));
  auto secondResult = retrieveForMutation(second);
  if (!secondResult) return std::unexpected(std::move(secondResult.error()));
  auto& firstItems = firstResult->get();
  auto& secondItems = secondResult->get();
  if (std::holds_alternative<BagSlot>(first)) {
    auto allowed = itemAllowedInBag(secondItems, std::get<BagSlot>(first).first);
    if (!allowed) return std::unexpected(std::move(allowed.error()));
    if (!*allowed) return false;
  }
  if (std::holds_alternative<BagSlot>(second)) {
    auto allowed = itemAllowedInBag(firstItems, std::get<BagSlot>(second).first);
    if (!allowed) return std::unexpected(std::move(allowed.error()));
    if (!*allowed) return false;
  }
  if (std::holds_alternative<EquipmentSlot>(first) && (itemSafeCount(secondItems) > 1 || !itemAllowedAsEquipment(secondItems, std::get<EquipmentSlot>(first))))
    return false;
  if (std::holds_alternative<EquipmentSlot>(second) && (itemSafeCount(firstItems) > 1 || !itemAllowedAsEquipment(firstItems, std::get<EquipmentSlot>(second))))
    return false;

  std::swap(firstItems, secondItems);
  swapCustomBarLinks(first, second);

  return true;
}

[[nodiscard]] std::expected<bool, StarException> PlayerInventory::setItem(InventorySlot const& slot, std::shared_ptr<Item> const& item) {
  if (item && item->empty())
    return setItem(slot, {});

  if (auto currencyItem = as<CurrencyItem>(item)) {
    m_currencies[currencyItem->currencyType()] += currencyItem->totalValue();
    return true;
  } else if (std::holds_alternative<EquipmentSlot>(slot)) {
    if (itemAllowedAsEquipment(item, std::get<EquipmentSlot>(slot))) {
      m_equipment[std::get<EquipmentSlot>(slot)] = item;
      return true;
    }
  } else if (std::holds_alternative<SwapSlot>(slot)) {
    m_swapSlot = item;
    return true;
  } else if (std::holds_alternative<TrashSlot>(slot)) {
    m_trashSlot = item;
    return true;
  } else {
    auto bs = std::get<BagSlot>(slot);
    auto allowed = itemAllowedInBag(item, bs.first);
    if (!allowed) return std::unexpected(std::move(allowed.error()));
    if (*allowed) {
      auto bag = m_bags.find_ref(bs.first);
      if (!bag) return std::unexpected(ItemException("Invalid inventory bag"));
      auto assigned = bag->get()->setItem(bs.second, item);
      if (!assigned) return std::unexpected(std::move(assigned.error()));
      return true;
    }
  }

  return false;
}

[[nodiscard]] std::expected<bool, StarException> PlayerInventory::consumeSlot(InventorySlot const& slot, std::uint64_t count) {
  if (count == 0)
    return true;

  auto result = retrieveForMutation(slot);
  if (!result) return std::unexpected(std::move(result.error()));
  auto& item = result->get();
  if (!item)
    return false;

  bool consumed = item->consume(count);
  normalizeItem(item);

  return consumed;
}

bool PlayerInventory::slotValid(InventorySlot const& slot) const {
  if (std::holds_alternative<BagSlot>(slot)) {
    auto const& bagSlot = std::get<BagSlot>(slot);
    if (auto bag = bagContents(bagSlot.first)) {
      if (static_cast<std::size_t>(bagSlot.second) >= bag->size())
        return false;
    }
    else return false;
  }
  return true;
}

[[nodiscard]] std::expected<std::shared_ptr<Item>, StarException> PlayerInventory::addItems(std::shared_ptr<Item> items) {
  if (!items || items->empty())
    return {};

  // First, add coins as monetary value.
  if (auto currencyItem = as<CurrencyItem>(items)) {
    auto added = addCurrency(currencyItem->currencyType(), currencyItem->totalValue());
    if (!added) return std::unexpected(std::move(added.error()));
    return {};
  }

  // Then, try adding equipment to the equipment slots.
  for (auto slot : {EquipmentSlot::Head, EquipmentSlot::Chest, EquipmentSlot::Legs, EquipmentSlot::Back}) {
    if (!items->empty() && itemAllowedAsEquipment(items, slot) && !equipment(slot)) {
      auto taken = items->take(1);
      if (!taken) return std::unexpected(std::move(taken.error()));
      m_equipment[slot] = std::move(*taken);
    }
  }

  if (is<MaterialItem>(items)) {
    auto primary = primaryHeldItem();
    if (!primary) return std::unexpected(std::move(primary.error()));
    if (*primary) {
      (*primary)->stackWith(optionalRef(items));
      if (items->empty()) return std::shared_ptr<Item>();
    }
  }

  // Then, finally the bags
  return addToBags(std::move(items));
}

[[nodiscard]] std::expected<std::shared_ptr<Item>, StarException> PlayerInventory::addToBags(std::shared_ptr<Item> items) {
  if (!items || items->empty())
    return {};

  for (auto pair : m_bags) {
    auto allowed = itemAllowedInBag(items, pair.first);
    if (!allowed) return std::unexpected(std::move(allowed.error()));
    if (!*allowed) continue;
    auto stacked = pair.second->stackItems(items);
    if (!stacked) return std::unexpected(std::move(stacked.error()));
    items = std::move(*stacked);
    if (!items) break;

    for (unsigned i = 0; i < pair.second->size(); ++i) {
      auto slot = std::as_const(*pair.second).at(i);
      if (!slot) return std::unexpected(std::move(slot.error()));
      if (!*slot) {
        auto assigned = pair.second->setItem(i, std::exchange(items, {}));
        if (!assigned) return std::unexpected(std::move(assigned.error()));
        auto added = autoAddToCustomBar(BagSlot(pair.first, i));
        if (!added) return std::unexpected(std::move(added.error()));
        break;
      }
    }
  }

  return items;
}

[[nodiscard]] std::expected<std::uint64_t, StarException> PlayerInventory::itemsCanFit(std::shared_ptr<Item> const& items) const {
  if (!items || items->empty())
    return 0;

  if (is<CurrencyItem>(items))
    return items->count();

  std::uint64_t canFit = 0;

  // First, check the equipment slots
  if (is<HeadArmor>(items) && !headArmor())
    ++canFit;
  if (is<ChestArmor>(items) && !chestArmor())
    ++canFit;
  if (is<LegsArmor>(items) && !legsArmor())
    ++canFit;
  if (is<BackArmor>(items) && !backArmor())
    ++canFit;

  // Then add into bags
  for (auto const& pair : m_bags) {
    auto allowed = itemAllowedInBag(items, pair.first);
    if (!allowed) return std::unexpected(std::move(allowed.error()));
    if (*allowed) canFit += pair.second->itemsCanFit(optionalRef(items));
  }

  return std::min(canFit, items->count());
}

bool PlayerInventory::hasItem(ItemDescriptor const& descriptor, bool exactMatch) const {
  return hasCountOfItem(descriptor, exactMatch) >= descriptor.count();
}

std::uint64_t PlayerInventory::hasCountOfItem(ItemDescriptor const& descriptor, bool exactMatch) const {
  auto one = descriptor.singular();

  std::uint64_t count = 0;
  auto countItem = [&](std::shared_ptr<Item> const& ptr) {
    if (ptr)
      count += ptr->matches(one, exactMatch) ? ptr->count() : 0;
  };

  countItem(m_swapSlot);
  countItem(m_trashSlot);
  for (auto const& p : m_equipment)
    countItem(p.second);

  for (auto const& pair : m_bags)
    count += pair.second->available(one, exactMatch);

  return count;
}

bool PlayerInventory::consumeItems(ItemDescriptor const& descriptor, bool exactMatch) {
  if (descriptor.count() == 0)
    return true;

  auto one = descriptor.singular();

  Map<String, std::uint64_t> consumeFromItemBags;
  for (auto const& pair : m_bags)
    consumeFromItemBags[pair.first] = pair.second->available(one);

  std::uint64_t consumeFromEquipment = 0;
  for (auto const& p : m_equipment) {
    if (p.second)
      consumeFromEquipment += p.second->matches(one, exactMatch) ? p.second->count() : 0;
  }

  std::uint64_t consumeFromSwap = 0;
  if (m_swapSlot)
    consumeFromSwap += m_swapSlot->matches(one, exactMatch) ? m_swapSlot->count() : 0;

  std::uint64_t consumeFromTrash = 0;
  if (m_trashSlot)
    consumeFromTrash += m_trashSlot->matches(one, exactMatch) ? m_trashSlot->count() : 0;

  auto totalAvailable = consumeFromEquipment + consumeFromSwap + consumeFromTrash;
  for (auto const& pair : consumeFromItemBags)
    totalAvailable += pair.second;

  if (totalAvailable < descriptor.count())
    return false;

  std::uint64_t leftoverCount = descriptor.count();
  std::uint64_t quantity;
  for (auto pair : m_bags) {
    quantity = std::min(leftoverCount, consumeFromItemBags[pair.first]);
    if (quantity > 0) {
      auto res = pair.second->consumeItems(one.multiply(quantity), exactMatch);
      ((void)(res));
      if constexpr (Star::DebugEnabled) {
        Star::star_assert_failed((res));
      }
      leftoverCount -= quantity;
    }
  }

  quantity = std::min(leftoverCount, consumeFromEquipment);
  if (quantity > 0) {
    auto leftoverQuantity = quantity;
    ((void)(leftoverQuantity));
    for (auto& p : m_equipment) {
      if (p.second && p.second->matches(one, exactMatch)) {
        auto toConsume = std::min(p.second->count(), quantity);
        auto res = p.second->consume(toConsume);
        normalizeItem(p.second);
        ((void)(res));
        if constexpr (Star::DebugEnabled) {
          Star::star_assert_failed((res));
        }

        leftoverQuantity -= toConsume;
      }
    }
    if constexpr (Star::DebugEnabled) {
      Star::star_assert_failed((leftoverQuantity == 0));
    }
    leftoverCount -= quantity;
  }

  quantity = std::min(leftoverCount, consumeFromSwap);
  if (quantity > 0) {
    if (m_swapSlot && m_swapSlot->matches(one, exactMatch)) {
      auto toConsume = std::min(m_swapSlot->count(), quantity);
      auto res = m_swapSlot->consume(toConsume);
      normalizeItem(m_swapSlot);
      ((void)(res));
      if constexpr (Star::DebugEnabled) {
        Star::star_assert_failed((res));
      }

      quantity -= toConsume;
      if constexpr (Star::DebugEnabled) {
        Star::star_assert_failed((quantity == 0));
      }
    }
    leftoverCount -= std::min(leftoverCount, consumeFromSwap);
  }

  quantity = std::min(leftoverCount, consumeFromTrash);
  if (quantity > 0) {
    if (m_trashSlot && m_trashSlot->matches(one, exactMatch)) {
      auto toConsume = std::min(m_trashSlot->count(), quantity);
      auto res = m_trashSlot->consume(toConsume);
      normalizeItem(m_trashSlot);
      ((void)(res));
      if constexpr (Star::DebugEnabled) {
        Star::star_assert_failed((res));
      }

      quantity -= toConsume;
      if constexpr (Star::DebugEnabled) {
        Star::star_assert_failed((quantity == 0));
      }
    }
    leftoverCount -= std::min(leftoverCount, consumeFromTrash);
  }

  if constexpr (Star::DebugEnabled) {
    Star::star_assert_failed((leftoverCount == 0));
  }
  return true;
}

[[nodiscard]] std::expected<ItemDescriptor, StarException> PlayerInventory::takeItems(ItemDescriptor const& descriptor, bool takePartial, bool exactMatch) {
  std::uint64_t hasCount = hasCountOfItem(descriptor, exactMatch);

  if (hasCount >= descriptor.count() || (takePartial && hasCount > 0)) {
    ItemDescriptor consumeDescriptor = descriptor.withCount(std::min(descriptor.count(), hasCount));
    consumeItems(consumeDescriptor, exactMatch);
    return consumeDescriptor;
  }

  return {};
}

[[nodiscard]] std::expected<HashMap<ItemDescriptor, std::uint64_t>, StarException> PlayerInventory::availableItems() const {
  auto items = allItems();
  if (!items) return std::unexpected(std::move(items.error()));
  return ItemDatabase::normalizeBag(*items);
}

std::shared_ptr<HeadArmor> PlayerInventory::headArmor() const {
  return as<HeadArmor>(equipment(EquipmentSlot::Head));
}

std::shared_ptr<ChestArmor> PlayerInventory::chestArmor() const {
  return as<ChestArmor>(equipment(EquipmentSlot::Chest));
}

std::shared_ptr<LegsArmor> PlayerInventory::legsArmor() const {
  return as<LegsArmor>(equipment(EquipmentSlot::Legs));
}

std::shared_ptr<BackArmor> PlayerInventory::backArmor() const {
  return as<BackArmor>(equipment(EquipmentSlot::Back));
}

std::shared_ptr<HeadArmor> PlayerInventory::headCosmetic() const {
  return as<HeadArmor>(equipment(EquipmentSlot::HeadCosmetic));
}

std::shared_ptr<ChestArmor> PlayerInventory::chestCosmetic() const {
  return as<ChestArmor>(equipment(EquipmentSlot::ChestCosmetic));
}

std::shared_ptr<LegsArmor> PlayerInventory::legsCosmetic() const {
  return as<LegsArmor>(equipment(EquipmentSlot::LegsCosmetic));
}

std::shared_ptr<BackArmor> PlayerInventory::backCosmetic() const {
  return as<BackArmor>(equipment(EquipmentSlot::BackCosmetic));
}

std::shared_ptr<ArmorItem> PlayerInventory::equipment(EquipmentSlot slot, bool testMask) const {
  if (testMask && !equipmentVisibility(slot))
    return {};

  auto item = m_equipment.value(slot);
  return item && !item->empty() ? as<ArmorItem>(item) : std::shared_ptr<ArmorItem>();
}

std::shared_ptr<ItemBag const> PlayerInventory::bagContents(String const& type) const {
  auto bag = m_bags.find_ref(type);
  return bag ? bag->get() : nullptr;
}

[[nodiscard]] std::expected<void, StarException> PlayerInventory::condenseBagStacks(String const& bagType) {
  auto entry = m_bags.find_ref(bagType);
  if (!entry) return std::unexpected(ItemException("Invalid inventory bag"));
  auto condensed = entry->get()->condenseStacks();
  if (!condensed) return std::unexpected(std::move(condensed.error()));
  for (std::size_t i = 0; i < m_customBar.size(0); ++i) {
    for (std::size_t j = 0; j < m_customBar.size(1); ++j) {
      auto& link = m_customBar(i, j);
      for (auto slotRef : {std::ref(link.first), std::ref(link.second)}) {
        auto& slot = slotRef.get();
        if (slot) {
          if (std::holds_alternative<BagSlot>(*slot) && std::get<BagSlot>(*slot).first == bagType) {
            auto item = std::as_const(*entry->get()).at(std::get<BagSlot>(*slot).second);
            if (!item) return std::unexpected(std::move(item.error()));
            if (!*item) slot.reset();
          }
        }
      }
    }
  }
  return {};
}

[[nodiscard]] std::expected<void, StarException> PlayerInventory::sortBag(String const& bagType) {
  auto entry = m_bags.find_ref(bagType);
  if (!entry) return std::unexpected(ItemException("Invalid inventory bag"));
  auto& bag = *entry->get();
  MultiArray<std::pair<std::shared_ptr<Item>, std::shared_ptr<Item>>, 2> saved(m_customBar.size());
  for (std::size_t i = 0; i < m_customBar.size(0); ++i) {
    for (std::size_t j = 0; j < m_customBar.size(1); ++j) {
      auto const& link = m_customBar(i, j);
      for (bool primary : {true, false}) {
        auto const& slot = primary ? link.first : link.second;
        if (slot) {
          if (std::holds_alternative<BagSlot>(*slot) && std::get<BagSlot>(*slot).first == bagType) {
            auto item = std::as_const(bag).at(std::get<BagSlot>(*slot).second);
            if (!item) return std::unexpected(std::move(item.error()));
            (primary ? saved(i, j).first : saved(i, j).second) = *item;
          }
        }
      }
    }
  }
  auto database = m_root.itemDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  Map<String, ItemType> types;
  for (auto const& item : bag.items()) {
    if (item && !types.contains(item->name())) {
      auto length = item->name().size();
      if (!length) return std::unexpected(std::move(length.error()));
      auto type = (*database)->itemType(item->name());
      if (!type) return std::unexpected(std::move(type.error()));
      types[item->name()] = *type;
    }
  }
  bag.items().sort([&types](auto const& a, auto const& b) {
    if (a && !b) return true;
    if (!a) return false;
    auto aType = types.value(a->name());
    auto bType = types.value(b->name());
    if (aType != bType) return aType < bType;
    if (a->rarity() != b->rarity()) return a->rarity() > b->rarity();
    if (a->name().utf8() != b->name().utf8()) return a->name().utf8() < b->name().utf8();
    return a->count() > b->count();
  });
  Map<std::shared_ptr<Item>, std::size_t> indexes;
  for (std::size_t i = 0; i < bag.size(); ++i)
    if (bag.items()[i]) indexes[bag.items()[i]] = i;
  for (std::size_t i = 0; i < m_customBar.size(0); ++i) {
    for (std::size_t j = 0; j < m_customBar.size(1); ++j) {
      auto const& old = saved(i, j);
      if (old.first) m_customBar(i, j).first.emplace(BagSlot(bagType, indexes.value(old.first)));
      if (old.second) m_customBar(i, j).second.emplace(BagSlot(bagType, indexes.value(old.second)));
    }
  }
  return {};
}

[[nodiscard]] std::expected<void, StarException> PlayerInventory::shiftSwap(InventorySlot const& slot) {
  normalizeItem(m_swapSlot);
  if (std::holds_alternative<EquipmentSlot>(slot)) {
    if (itemAllowedAsEquipment(m_swapSlot, std::get<EquipmentSlot>(slot))) {
      auto result = retrieveForMutation(slot);
      if (!result) return std::unexpected(std::move(result.error()));
      auto& equipSlot = result->get();
      if (itemSafeCount(m_swapSlot) <= 1) {
        std::swap(m_swapSlot, equipSlot);
        swapCustomBarLinks(SwapSlot(), slot);
      } else if (itemSafeCount(equipSlot) == 0) {
        auto taken = m_swapSlot->take(1);
        if (!taken) return std::unexpected(std::move(taken.error()));
        equipSlot = std::move(*taken);
      }
    }
  } else if (std::holds_alternative<TrashSlot>(slot)) {
    normalizeItem(m_trashSlot);
    std::swap(m_swapSlot, m_trashSlot);
    swapCustomBarLinks(SwapSlot(), slot);
  } else if (std::holds_alternative<BagSlot>(slot)) {
    auto const& bs = std::get<BagSlot>(slot);
    auto allowed = itemAllowedInBag(m_swapSlot, bs.first);
    if (!allowed) return std::unexpected(std::move(allowed.error()));
    if (*allowed) {
      auto bag = m_bags.find_ref(bs.first);
      if (!bag) return std::unexpected(ItemException("Invalid inventory bag"));
      auto swapped = bag->get()->swapItems(bs.second, m_swapSlot);
      if (!swapped) return std::unexpected(std::move(swapped.error()));
      m_swapSlot = std::move(*swapped);
      swapCustomBarLinks(SwapSlot(), slot);
    }
  }
  normalizeItem(m_swapSlot);

  if (!m_swapSlot)
    m_swapReturnSlot = {};
  else
    m_swapReturnSlot = slot;
  return {};
}

[[nodiscard]] std::expected<bool, StarException> PlayerInventory::clearSwap() {
  normalizeItem(m_swapSlot);
  auto trySlot = [&](InventorySlot slot) -> std::expected<void, StarException> {
    if (!m_swapSlot) return {};
    auto stacked = stackWith(slot, m_swapSlot);
    if (!stacked) return std::unexpected(std::move(stacked.error()));
    m_swapSlot = std::move(*stacked);
    if (!m_swapSlot) swapCustomBarLinks(SwapSlot(), slot);
    return {};
  };
  if (m_swapReturnSlot) {
    auto slot = std::move(*m_swapReturnSlot);
    m_swapReturnSlot.reset();
    auto tried = trySlot(slot);
    if (!tried) return std::unexpected(std::move(tried.error()));
  }
  for (auto slot : {EquipmentSlot::Head, EquipmentSlot::Chest, EquipmentSlot::Legs, EquipmentSlot::Back}) {
    auto tried = trySlot(slot);
    if (!tried) return std::unexpected(std::move(tried.error()));
  }
  for (auto const& bag : m_bags) {
    for (unsigned i = 0; i < bag.second->size() && m_swapSlot; ++i) {
      auto allowed = itemAllowedInBag(m_swapSlot, bag.first);
      if (!allowed) return std::unexpected(std::move(allowed.error()));
      if (!*allowed) break;
      auto tried = trySlot(BagSlot(bag.first, i));
      if (!tried) return std::unexpected(std::move(tried.error()));
    }
  }
  return !m_swapSlot;
}

std::shared_ptr<Item> PlayerInventory::swapSlotItem() const {
  return m_swapSlot && !m_swapSlot->empty() ? m_swapSlot : std::shared_ptr<Item>();
}

[[nodiscard]] std::expected<void, StarException> PlayerInventory::setSwapSlotItem(std::shared_ptr<Item> const& items) {
  if (auto currencyItem = as<CurrencyItem>(items)) {
    auto added = addCurrency(currencyItem->currencyType(), currencyItem->totalValue());
    if (!added) return std::unexpected(std::move(added.error()));
    m_swapSlot = {};
  } else {
    m_swapSlot = items;
    normalizeItem(m_swapSlot);
    auto added = autoAddToCustomBar(SwapSlot());
    if (!added) return std::unexpected(std::move(added.error()));
  }
  return {};
}

std::shared_ptr<Item> PlayerInventory::essentialItem(EssentialItem essentialItem) const {
  return m_essential.value(essentialItem);
}

void PlayerInventory::setEssentialItem(EssentialItem essentialItem, std::shared_ptr<Item> item) {
  m_essential[essentialItem] = std::move(item);
}

StringMap<std::uint64_t> PlayerInventory::availableCurrencies() const {
  return m_currencies;
}

std::uint64_t PlayerInventory::currency(String const& currencyType) const {
  return m_currencies.value(currencyType, 0);
}

[[nodiscard]] std::expected<void, StarException> PlayerInventory::addCurrency(String const& currencyType, std::uint64_t amount) {
  std::uint64_t previousTotal = m_currencies[currencyType];
  std::uint64_t newTotal = previousTotal + amount;
  if (newTotal < previousTotal)
    newTotal = std::numeric_limits<std::uint64_t>::max();
  auto assets = m_root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/currencies.config");
  if (!config) return std::unexpected(std::move(config.error()));
  auto currency = config->get(currencyType);
  if (!currency) return std::unexpected(std::move(currency.error()));
  auto maximum = currency->getUInt("playerMax", std::numeric_limits<std::uint64_t>::max());
  if (!maximum) return std::unexpected(std::move(maximum.error()));
  m_currencies[currencyType] = std::min(*maximum, newTotal);
  return {};
}

bool PlayerInventory::consumeCurrency(String const& currencyType, std::uint64_t amount) {
  if (m_currencies[currencyType] >= amount) {
    m_currencies[currencyType] -= amount;
    return true;
  } else {
    return false;
  }
}

[[nodiscard]] std::expected<std::optional<InventorySlot>, StarException> PlayerInventory::customBarPrimarySlot(CustomBarIndex index) const {
  auto link = m_customBar.at(m_customBarGroup, index);
  if (!link) return std::unexpected(std::move(link.error()));
  return link->get().first;
}

[[nodiscard]] std::expected<std::optional<InventorySlot>, StarException> PlayerInventory::customBarSecondarySlot(CustomBarIndex index) const {
  auto link = m_customBar.at(m_customBarGroup, index);
  if (!link) return std::unexpected(std::move(link.error()));
  return link->get().second;
}

[[nodiscard]] std::expected<void, StarException> PlayerInventory::setCustomBarPrimarySlot(CustomBarIndex index, std::optional<InventorySlot> slot) {
  if (slot) {
    auto item = itemsAt(*slot);
    if (!item) return std::unexpected(std::move(item.error()));
    if (!*item) slot.reset();
  }
  auto link = m_customBar.at(m_customBarGroup, index);
  if (!link) return std::unexpected(std::move(link.error()));
  auto& cbl = link->get();
  if (slot && cbl.second == slot) std::swap(cbl.first, cbl.second);
  else cbl.first = slot;
  return {};
}

[[nodiscard]] std::expected<void, StarException> PlayerInventory::setCustomBarSecondarySlot(CustomBarIndex index, std::optional<InventorySlot> slot) {
  auto link = m_customBar.at(m_customBarGroup, index);
  if (!link) return std::unexpected(std::move(link.error()));
  auto& cbl = link->get();
  if (slot) {
    auto item = itemsAt(*slot);
    if (!item) return std::unexpected(std::move(item.error()));
    if (!*item || itemSafeTwoHanded(*item)) slot.reset();
  }
  bool primaryTwoHanded = false;
  if (cbl.first) {
    auto item = itemsAt(*cbl.first);
    if (!item) return std::unexpected(std::move(item.error()));
    primaryTwoHanded = itemSafeTwoHanded(*item);
  }
  if (cbl.first && cbl.first == slot && !primaryTwoHanded) std::swap(cbl.first, cbl.second);
  else {
    cbl.second = slot;
    if (primaryTwoHanded) cbl.first.reset();
  }
  return {};
}

std::expected<void, StarException> PlayerInventory::linkCustomBarSwap(CustomBarIndex index, bool primary) {
  if (auto item = swapSlotItem()) {
    auto linked = primary || item->twoHanded()
      ? setCustomBarPrimarySlot(index, InventorySlot(SwapSlot()))
      : setCustomBarSecondarySlot(index, InventorySlot(SwapSlot()));
    if (!linked) return std::unexpected(std::move(linked.error()));
    return clearSwap().transform([](bool) {});
  }
  auto slot = m_customBar.at(m_customBarGroup, index);
  if (!slot) return std::unexpected(std::move(slot.error()));
  selectActionBarLocation(index);
  return {};
}

std::expected<void, StarException> PlayerInventory::clearCustomBarLink(CustomBarIndex index, bool primary) {
  auto primarySlot = customBarPrimarySlot(index);
  if (!primarySlot) return std::unexpected(std::move(primarySlot.error()));
  auto secondarySlot = customBarSecondarySlot(index);
  if (!secondarySlot) return std::unexpected(std::move(secondarySlot.error()));
  if (!primary && *primarySlot) {
    auto item = itemsAt(**primarySlot);
    if (!item) return std::unexpected(std::move(item.error()));
    primary = itemSafeTwoHanded(*item);
  }
  return primary ? setCustomBarPrimarySlot(index, {}) : setCustomBarSecondarySlot(index, {});
}

[[nodiscard]] std::expected<void, StarException> PlayerInventory::addToCustomBar(InventorySlot slot) {
  for (std::size_t j = 0; j < m_customBar.size(1); ++j) {
    auto result = m_customBar.at(m_customBarGroup, j);
    if (!result) return std::unexpected(std::move(result.error()));
    auto& cbl = result->get();
    if (!cbl.first && !cbl.second) {
      cbl.first.emplace(slot);
      break;
    }
  }
  return {};
}

std::uint8_t PlayerInventory::customBarGroup() const {
  return m_customBarGroup;
}

void PlayerInventory::setCustomBarGroup(std::uint8_t group) {
  m_customBarGroup = group;
}

std::uint8_t PlayerInventory::customBarGroups() const {
  return m_customBar.size(0);
}

std::uint8_t PlayerInventory::customBarIndexes() const {
  return m_customBar.size(1);
}

SelectedActionBarLocation PlayerInventory::selectedActionBarLocation() const {
  return m_selectedActionBar;
}

void PlayerInventory::selectActionBarLocation(SelectedActionBarLocation location) {
  m_selectedActionBar = location;
}

[[nodiscard]] std::expected<std::shared_ptr<Item>, StarException> PlayerInventory::primaryHeldItem() const {
  if (auto swapItem = swapSlotItem())
    return swapItem;

  if (std::holds_alternative<EssentialItem>(m_selectedActionBar))
    return m_essential.value(std::get<EssentialItem>(m_selectedActionBar));

  if (std::holds_alternative<CustomBarIndex>(m_selectedActionBar)) {
    auto slot = customBarPrimarySlot(std::get<CustomBarIndex>(m_selectedActionBar));
    if (!slot) return std::unexpected(std::move(slot.error()));
    if (*slot) return itemsAt(**slot);
  }

  return {};
}

[[nodiscard]] std::expected<std::shared_ptr<Item>, StarException> PlayerInventory::secondaryHeldItem() const {
  auto primary = primaryHeldItem();
  if (!primary) return std::unexpected(std::move(primary.error()));
  if (itemSafeTwoHanded(*primary) || swapSlotItem() || !std::holds_alternative<CustomBarIndex>(m_selectedActionBar))
    return std::shared_ptr<Item>();
  auto link = m_customBar.at(m_customBarGroup, std::get<CustomBarIndex>(m_selectedActionBar));
  if (!link) return std::unexpected(std::move(link.error()));
  auto const& cbl = link->get();
  if (cbl.first) {
    auto item = itemsAt(*cbl.first);
    if (!item) return std::unexpected(std::move(item.error()));
    if (itemSafeTwoHanded(*item)) return std::shared_ptr<Item>();
  }
  if (cbl.second) return itemsAt(*cbl.second);
  return std::shared_ptr<Item>();
}

[[nodiscard]] std::expected<std::optional<InventorySlot>, StarException> PlayerInventory::primaryHeldSlot() const {
  if (swapSlotItem())
    return std::optional<InventorySlot>(SwapSlot());
  if (std::holds_alternative<CustomBarIndex>(m_selectedActionBar))
    return customBarPrimarySlot(std::get<CustomBarIndex>(m_selectedActionBar));
  return {};
}

[[nodiscard]] std::expected<std::optional<InventorySlot>, StarException> PlayerInventory::secondaryHeldSlot() const {
  auto primary = primaryHeldItem();
  if (!primary) return std::unexpected(std::move(primary.error()));
  if (swapSlotItem() || itemSafeTwoHanded(*primary))
    return std::optional<InventorySlot>();
  if (std::holds_alternative<CustomBarIndex>(m_selectedActionBar))
    return customBarSecondarySlot(std::get<CustomBarIndex>(m_selectedActionBar));
  return {};
}

List<std::shared_ptr<Item>> PlayerInventory::pullOverflow() {
  return std::move(m_inventoryLoadOverflow);
}

bool PlayerInventory::equipmentVisibility(EquipmentSlot slot) const {
  return (m_equipmentVisibilityMask >> static_cast<std::uint8_t>(slot)) & 0x1;
}

void PlayerInventory::setEquipmentVisibility(EquipmentSlot slot, bool visible) {
  if (visible)
    m_equipmentVisibilityMask |= (1 << static_cast<std::uint8_t>(slot));
  else
    m_equipmentVisibilityMask &= ~(1 << static_cast<std::uint8_t>(slot));
}

[[nodiscard]] std::expected<void, StarException> PlayerInventory::load(Json const& store) {
  auto database = m_root.itemDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  auto loadItem = [&](String const& key, std::shared_ptr<Item>& item, bool optional = false) -> std::expected<void, StarException> {
    auto json = optional ? store.get(key, Json()) : store.get(key);
    if (!json) return std::unexpected(std::move(json.error()));
    auto loaded = (*database)->diskLoad(*json);
    if (!loaded) return std::unexpected(std::move(loaded.error()));
    item = std::move(*loaded);
    normalizeItem(item);
    return {};
  };
  if (!EquipmentSlotNames) return std::unexpected(EquipmentSlotNames.error());
  for (auto const& slot : *EquipmentSlotNames) {
    auto loaded = loadItem(Star::format("{}Slot", slot.second), m_equipment[slot.first], true);
    if (!loaded) return std::unexpected(std::move(loaded.error()));
  }
  auto bags = store.getObject("itemBags");
  if (!bags) return std::unexpected(std::move(bags.error()));
  m_inventoryLoadOverflow.clear();
  for (auto const& entry : *bags) {
    auto loaded = ItemBag::loadStore(m_root, entry.second);
    if (!loaded) return std::unexpected(std::move(loaded.error()));
    loaded->cleanup();
    if (auto bag = m_bags.find_ref(entry.first)) {
      auto overflow = loaded->resize(bag->get()->size());
      if (!overflow) return std::unexpected(std::move(overflow.error()));
      m_inventoryLoadOverflow.append_range(*overflow);
      *bag->get() = std::move(*loaded);
    } else
      m_inventoryLoadOverflow.append_range(loaded->items());
  }
  auto swap = loadItem("swapSlot", m_swapSlot);
  if (!swap) return std::unexpected(std::move(swap.error()));
  auto trash = loadItem("trashSlot", m_trashSlot);
  if (!trash) return std::unexpected(std::move(trash.error()));
  auto currencies = store.getObject("currencies");
  if (!currencies) return std::unexpected(std::move(currencies.error()));
  m_currencies.clear();
  for (auto const& entry : *currencies) {
    auto count = entry.second.toUInt();
    if (!count) return std::unexpected(std::move(count.error()));
    m_currencies[entry.first] = *count;
  }
  auto group = store.getUInt("customBarGroup");
  if (!group) return std::unexpected(std::move(group.error()));
  m_customBarGroup = *group;
  auto bar = store.get("customBar");
  if (!bar) return std::unexpected(std::move(bar.error()));
  for (std::size_t i = 0; i < m_customBar.size(0); ++i) {
    auto row = bar->get(i, JsonArray());
    if (!row) return std::unexpected(std::move(row.error()));
    for (std::size_t j = 0; j < m_customBar.size(1); ++j) {
      auto links = row->get(j, JsonArray());
      if (!links) return std::unexpected(std::move(links.error()));
      for (std::size_t hand = 0; hand < 2; ++hand) {
        auto json = links->get(hand, {});
        if (!json) return std::unexpected(std::move(json.error()));
        std::optional<InventorySlot> link;
        if (!json->isNull()) {
          auto parsed = jsonToInventorySlot(*json);
          if (!parsed) return std::unexpected(std::move(parsed.error()));
          if (slotValid(*parsed)) link = std::move(*parsed);
        }
        (hand == 0 ? m_customBar(i, j).first : m_customBar(i, j).second) = std::move(link);
      }
    }
  }
  auto selected = store.get("selectedActionBar");
  if (!selected) return std::unexpected(std::move(selected.error()));
  auto location = jsonToSelectedActionBarLocation(*selected);
  if (!location) return std::unexpected(std::move(location.error()));
  m_selectedActionBar = std::move(*location);
  m_essential.clear();
  for (auto const& entry : {std::pair<EssentialItem, String>{EssentialItem::BeamAxe, "beamAxe"}, {EssentialItem::WireTool, "wireTool"},
      {EssentialItem::PaintTool, "paintTool"}, {EssentialItem::InspectionTool, "inspectionTool"}}) {
    auto loaded = loadItem(entry.second, m_essential[entry.first]);
    if (!loaded) return std::unexpected(std::move(loaded.error()));
  }
  auto visibility = store.optUInt("equipmentVisibilityMask");
  if (!visibility) return std::unexpected(std::move(visibility.error()));
  m_equipmentVisibilityMask = static_cast<unsigned>(visibility->value_or(0xFFFFFFFF));
  return {};
}

[[nodiscard]] std::expected<Json, StarException> PlayerInventory::store() const {
  auto database = m_root.itemDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  JsonArray bar;
  for (std::size_t i = 0; i < m_customBar.size(0); ++i) {
    JsonArray row;
    for (std::size_t j = 0; j < m_customBar.size(1); ++j) {
      auto const& link = m_customBar(i, j);
      JsonArray hands;
      for (auto slotRef : {std::cref(link.first), std::cref(link.second)}) {
        auto const& slot = slotRef.get();
        Json json;
        if (slot) {
          auto encoded = jsonFromInventorySlot(*slot);
          if (!encoded) return std::unexpected(std::move(encoded.error()));
          json = std::move(*encoded);
        }
        hands.push_back(std::move(json));
      }
      row.push_back(std::move(hands));
    }
    bar.push_back(std::move(row));
  }
  JsonObject bags;
  for (auto const& bag : m_bags) {
    auto encoded = bag.second->diskStore();
    if (!encoded) return std::unexpected(std::move(encoded.error()));
    bags[bag.first] = std::move(*encoded);
  }
  JsonObject currencies;
  for (auto const& currency : m_currencies) currencies[currency.first] = currency.second;
  auto selected = jsonFromSelectedActionBarLocation(m_selectedActionBar);
  if (!selected) return std::unexpected(std::move(selected.error()));
  JsonObject data{{"itemBags", std::move(bags)}, {"currencies", std::move(currencies)},
    {"customBarGroup", m_customBarGroup}, {"customBar", std::move(bar)},
    {"selectedActionBar", std::move(*selected)}, {"equipmentVisibilityMask", m_equipmentVisibilityMask}};
  auto saveItem = [&](String const& key, std::shared_ptr<Item> const& item) -> std::expected<void, StarException> {
    auto encoded = (*database)->diskStore(item);
    if (!encoded) return std::unexpected(std::move(encoded.error()));
    data[key] = std::move(*encoded);
    return {};
  };
  auto swap = saveItem("swapSlot", m_swapSlot);
  if (!swap) return std::unexpected(std::move(swap.error()));
  auto trash = saveItem("trashSlot", m_trashSlot);
  if (!trash) return std::unexpected(std::move(trash.error()));
  for (auto const& entry : {std::pair<EssentialItem, String>{EssentialItem::BeamAxe, "beamAxe"}, {EssentialItem::WireTool, "wireTool"},
      {EssentialItem::PaintTool, "paintTool"}, {EssentialItem::InspectionTool, "inspectionTool"}}) {
    auto saved = saveItem(entry.second, m_essential.value(entry.first));
    if (!saved) return std::unexpected(std::move(saved.error()));
  }
  if (!EquipmentSlotNames) return std::unexpected(EquipmentSlotNames.error());
  for (auto const& equipment : m_equipment) {
    if (equipment.first <= EquipmentSlot::BackCosmetic || equipment.second) {
      auto name = EquipmentSlotNames->get_right(equipment.first);
      if (!name) return std::unexpected(std::move(name.error()));
      auto saved = saveItem(Star::format("{}Slot", name->get()), equipment.second);
      if (!saved) return std::unexpected(std::move(saved.error()));
    }
  }
  return data;
}

[[nodiscard]] std::expected<void, StarException> PlayerInventory::forEveryItem(std::function<std::expected<void, StarException>(InventorySlot const&, std::shared_ptr<Item>&)> function) {
  if (!function) return std::unexpected(InventoryException("Empty inventory callback"));
  auto visit = [&](InventorySlot const& slot, std::shared_ptr<Item>& item) -> std::expected<void, StarException> {
    normalizeItem(item);
    if (!item) return {};
    auto result = function(slot, item);
    normalizeItem(item);
    return result;
  };
  for (auto& item : m_equipment) {
    auto result = visit(item.first, item.second);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  for (auto const& bag : m_bags) {
    for (std::size_t i = 0; i < bag.second->size(); ++i) {
      auto item = bag.second->at(i);
      if (!item) return std::unexpected(std::move(item.error()));
      auto result = visit(BagSlot(bag.first, i), item->get());
      if (!result) return std::unexpected(std::move(result.error()));
    }
  }
  auto swap = visit(SwapSlot(), m_swapSlot);
  if (!swap) return std::unexpected(std::move(swap.error()));
  return visit(TrashSlot(), m_trashSlot);
}

[[nodiscard]] std::expected<void, StarException> PlayerInventory::forEveryItem(std::function<std::expected<void, StarException>(InventorySlot const&, std::shared_ptr<Item> const&)> function) const {
  if (!function) return std::unexpected(InventoryException("Empty inventory callback"));
  auto visit = [&](InventorySlot const& slot, std::shared_ptr<Item> const& item) -> std::expected<void, StarException> {
    if (item && !item->empty()) return function(slot, item);
    return {};
  };
  for (auto const& item : m_equipment) {
    auto result = visit(item.first, item.second);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  for (auto const& bag : m_bags) {
    auto const& contents = std::as_const(*bag.second);
    for (std::size_t i = 0; i < contents.size(); ++i) {
      auto item = contents.at(i);
      if (!item) return std::unexpected(std::move(item.error()));
      auto result = visit(BagSlot(bag.first, i), *item);
      if (!result) return std::unexpected(std::move(result.error()));
    }
  }
  auto swap = visit(SwapSlot(), m_swapSlot);
  if (!swap) return std::unexpected(std::move(swap.error()));
  return visit(TrashSlot(), m_trashSlot);
}

[[nodiscard]] std::expected<List<std::shared_ptr<Item>>, StarException> PlayerInventory::allItems() const {
  List<std::shared_ptr<Item>> items;
  auto result =
    forEveryItem([&](InventorySlot const&, std::shared_ptr<Item> const& item) -> std::expected<void, StarException> {
      items.push_back(item);
      return {};
    });
  if (!result) return std::unexpected(std::move(result.error()));
  return items;
}

[[nodiscard]] std::expected<Map<String, std::uint64_t>, StarException> PlayerInventory::itemSummary() const {
  Map<String, std::uint64_t> summary;
  auto result = forEveryItem([&](InventorySlot const&, std::shared_ptr<Item> const& item) -> std::expected<void, StarException> {
    summary[item->name()] += item->count();
    return {};
  });
  if (!result) return std::unexpected(std::move(result.error()));
  return summary;
}

[[nodiscard]] std::expected<void, StarException> PlayerInventory::cleanup() {
  for (auto const& bag : m_bags) bag.second->cleanup();
  for (auto& item : m_equipment) normalizeItem(item.second);
  normalizeItem(m_swapSlot);
  normalizeItem(m_trashSlot);
  for (std::size_t i = 0; i < m_customBar.size(0); ++i) {
    for (std::size_t j = 0; j < m_customBar.size(1); ++j) {
      auto& link = m_customBar(i, j);
      std::shared_ptr<Item> primary, secondary;
      if (link.first) {
        auto item = retrieve(*link.first);
        if (!item) return std::unexpected(std::move(item.error()));
        primary = *item;
      }
      if (link.second) {
        auto item = retrieve(*link.second);
        if (!item) return std::unexpected(std::move(item.error()));
        secondary = *item;
      }
      if (!primary) link.first.reset();
      if (!secondary || itemSafeTwoHanded(primary) || itemSafeTwoHanded(secondary)) link.second.reset();
    }
  }
  return {};
}

void PlayerInventory::setPlayer(std::optional<std::reference_wrapper<Player>> player) {
  m_player = player;
}

[[nodiscard]] std::expected<std::reference_wrapper<PlayerInventory const>, StarException> PlayerInventory::blankInventory() const {
  if (!m_blankInventory) {
    auto blank = create(m_root, m_inventoryConfig, m_currenciesConfig);
    if (!blank) return std::unexpected(std::move(blank.error()));
    m_blankInventory = std::move(*blank);
  }
  return std::cref(*m_blankInventory);
}

[[nodiscard]] std::expected<void, StarException> PlayerInventory::netStore(DataStream& ds, NetCompatibilityRules rules) const {
  if (m_player && !m_player->get().isMaster() && !rules.isAdmin()) {
    auto blank = blankInventory();
    if (!blank) return std::unexpected(std::move(blank.error()));
    return blank->get().netStore(ds, rules);
  }
  return NetElementSyncGroup::netStore(ds, rules);
}

[[nodiscard]] std::expected<bool, StarException> PlayerInventory::writeNetDelta(DataStream& ds, std::uint64_t version, NetCompatibilityRules rules) const {
  if (m_player && !m_player->get().isMaster() && !rules.isAdmin()) {
    auto blank = blankInventory();
    if (!blank) return std::unexpected(std::move(blank.error()));
    return blank->get().writeNetDelta(ds, version, rules);
  }
  return NetElementSyncGroup::writeNetDelta(ds, version, rules);
}

[[nodiscard]] std::expected<bool, StarException> PlayerInventory::checkInventoryFilter(std::shared_ptr<Item> const& items, String const& filterName) const {
  Json filterConfig;
  auto filters = items->instanceValue("inventoryFilters");
  if (!filters) return std::unexpected(std::move(filters.error()));
  if (filters->isType(Json::Type::Object)) {
    auto selected = filters->opt(filterName);
    if (!selected) return std::unexpected(std::move(selected.error()));
    filterConfig = selected->value_or(Json{});
    if (!filterConfig.isType(Json::Type::Object)) {
      auto fallback = filters->opt("default");
      if (!fallback) return std::unexpected(std::move(fallback.error()));
      filterConfig = fallback->value_or(Json{});
    }
  }
  if (!filterConfig.isType(Json::Type::Object)) {
    auto assets = m_root.assets();
    if (!assets) return std::unexpected(std::move(assets.error()));
    auto config = (*assets)->json("/player.config:inventoryFilters");
    if (!config) return std::unexpected(std::move(config.error()));
    auto selected = config->opt(filterName);
    if (!selected) return std::unexpected(std::move(selected.error()));
    filterConfig = selected->value_or(Json{});
    if (!filterConfig.isType(Json::Type::Object)) {
      auto fallback = config->get("default", JsonObject());
      if (!fallback) return std::unexpected(std::move(fallback.error()));
      filterConfig = std::move(*fallback);
    }
  }
  auto database = m_root.itemDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  auto type = (*database)->itemType(items->name());
  if (!type) return std::unexpected(std::move(type.error()));
  auto typeName = enumName<ItemTypeNames>(*type);
  if (!typeName) return std::unexpected(std::move(typeName.error()));
  for (auto const& entry : {std::pair<String, bool>{"typeWhitelist", false}, {"typeBlacklist", true}}) {
    auto present = filterConfig.contains(entry.first);
    if (!present) return std::unexpected(std::move(present.error()));
    if (*present) {
      auto list = filterConfig.getArray(entry.first);
      if (!list) return std::unexpected(std::move(list.error()));
      if (list->contains(Json(String(*typeName))) == entry.second)
        return false;
    }
  }
  auto tags = (*database)->itemTags(items->name());
  if (!tags) return std::unexpected(std::move(tags.error()));
  for (auto const& entry : {std::pair<String, bool>{"tagWhitelist", false}, {"tagBlacklist", true}}) {
    auto present = filterConfig.contains(entry.first);
    if (!present) return std::unexpected(std::move(present.error()));
    if (*present) {
      auto list = filterConfig.getArray(entry.first);
      if (!list) return std::unexpected(std::move(list.error()));
      bool matched = false;
      for (auto const& value : *list) {
        auto tag = value.toString();
        if (!tag) return std::unexpected(std::move(tag.error()));
        if (tags->contains(*tag)) matched = true;
      }
      if (matched == entry.second) return false;
    }
  }
  for (auto const& entry : {std::pair<String, bool>{"categoryWhitelist", false}, {"categoryBlacklist", true}}) {
    auto list = filterConfig.optArray(entry.first);
    if (!list) return std::unexpected(std::move(list.error()));
    if (*list) {
      auto categories = jsonToStringSet(**list);
      if (!categories) return std::unexpected(std::move(categories.error()));
      if (categories->contains(items->category()) == entry.second) return false;
    }
  }
  return true;
}

void PlayerInventory::normalizeItem(std::shared_ptr<Item>& item) {
  if (item && item->empty())
    item.reset();
}

[[nodiscard]] std::expected<std::shared_ptr<Item>, StarException> PlayerInventory::retrieve(InventorySlot const& slot) const {
  if (std::holds_alternative<EquipmentSlot>(slot)) {
    auto item = m_equipment.find_ref(std::get<EquipmentSlot>(slot));
    return item && item->get() && !item->get()->empty() ? item->get() : std::shared_ptr<Item>();
  } else if (std::holds_alternative<BagSlot>(slot)) {
    auto const& bs = std::get<BagSlot>(slot);
    if (auto bag = m_bags.find_ref(bs.first))
      return std::as_const(*bag->get()).at(bs.second);
  } else {
    auto const& item = std::holds_alternative<SwapSlot>(slot) ? m_swapSlot : m_trashSlot;
    return item && !item->empty() ? item : std::shared_ptr<Item>();
  }

  auto json = jsonFromInventorySlot(slot);
  if (!json) return std::unexpected(std::move(json.error()));
  auto diagnostic = json->repr();
  if (!diagnostic) return std::unexpected(std::move(diagnostic.error()));
  return std::unexpected(ItemException::format("Invalid inventory slot {}", *diagnostic));
}

[[nodiscard]] std::expected<std::reference_wrapper<std::shared_ptr<Item>>, StarException> PlayerInventory::retrieveForMutation(InventorySlot const& slot) {
  auto item = [&]() -> std::expected<std::reference_wrapper<std::shared_ptr<Item>>, StarException> {
    if (std::holds_alternative<EquipmentSlot>(slot))
      return std::ref(m_equipment[std::get<EquipmentSlot>(slot)]);
    if (std::holds_alternative<BagSlot>(slot)) {
      auto const& position = std::get<BagSlot>(slot);
      auto bag = m_bags.find_ref(position.first);
      if (!bag) {
        auto json = jsonFromInventorySlot(slot);
        if (!json) return std::unexpected(std::move(json.error()));
        auto diagnostic = json->repr();
        if (!diagnostic) return std::unexpected(std::move(diagnostic.error()));
        return std::unexpected(ItemException::format("Invalid inventory slot {}", *diagnostic));
      }
      return bag->get()->at(position.second);
    }
    return std::ref(std::holds_alternative<SwapSlot>(slot) ? m_swapSlot : m_trashSlot);
  }();
  if (!item) return std::unexpected(std::move(item.error()));
  normalizeItem(item->get());
  return item;
}

void PlayerInventory::swapCustomBarLinks(InventorySlot a, InventorySlot b) {
  m_customBar.forEach([&](Array2S const&, CustomBarLink& p) {
      if (p.first == a)
        p.first = b;
      else if (p.first == b)
        p.first = a;

      if (p.second == a)
        p.second = b;
      else if (p.second == b)
        p.second = a;
    });
}

[[nodiscard]] std::expected<void, StarException> PlayerInventory::autoAddToCustomBar(InventorySlot slot) {
  auto configuration = m_root.configuration();
  if (!configuration) return std::unexpected(std::move(configuration.error()));
  auto config = (*configuration)->getPath("inventory.pickupToActionBar");
  if (!config) return std::unexpected(std::move(config.error()));
  auto enabled = config->toBool();
  if (!enabled) return std::unexpected(std::move(enabled.error()));
  if (!*enabled) return {};
  auto items = itemsAt(slot);
  if (!items) return std::unexpected(std::move(items.error()));
  if (*items && !(*items)->empty()) {
    auto allowed = checkInventoryFilter(*items, "autoAddToCustomBar");
    if (!allowed) return std::unexpected(std::move(allowed.error()));
    if (*allowed) return addToCustomBar(slot);
  }
  return {};
}

[[nodiscard]] std::expected<void, StarException> PlayerInventory::netElementsNeedLoad(bool) {
  auto database = m_root.itemDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  auto load = [&](NetElementData<ItemDescriptor>& state, std::shared_ptr<Item>& item) -> std::expected<void, StarException> {
    if (state.pullUpdated()) {
      auto result = (*database)->loadItem(state.get(), item);
      if (!result) return std::unexpected(std::move(result.error()));
    }
    return {};
  };
  for (auto& entry : m_equipmentNetState) {
    auto result = load(entry.second, m_equipment[entry.first]);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  for (auto& bag : m_bagsNetState) {
    auto& items = m_bags[bag.first]->items();
    for (std::size_t i = 0; i < bag.second.size(); ++i) {
      auto result = load(bag.second[i], items[i]);
      if (!result) return std::unexpected(std::move(result.error()));
    }
  }
  auto swap = load(m_swapSlotNetState, m_swapSlot);
  if (!swap) return std::unexpected(std::move(swap.error()));
  auto trash = load(m_trashSlotNetState, m_trashSlot);
  if (!trash) return std::unexpected(std::move(trash.error()));
  m_currencies = m_currenciesNetState.get();
  m_customBarGroup = m_customBarGroupNetState.get();
  for (std::size_t i = 0; i < m_customBar.size(0); ++i)
    for (std::size_t j = 0; j < m_customBar.size(1); ++j)
      m_customBar(i, j) = m_customBarNetState(i, j).get();
  m_selectedActionBar = m_selectedActionBarNetState.get();
  for (auto& entry : m_essentialNetState) {
    auto result = load(entry.second, m_essential[entry.first]);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  return cleanup();
}

[[nodiscard]] std::expected<void, StarException> PlayerInventory::netElementsNeedStore() {
  auto cleaned = cleanup();
  if (!cleaned) return std::unexpected(std::move(cleaned.error()));
  for (auto& entry : m_equipmentNetState) entry.second.set(itemSafeDescriptor(m_equipment.value(entry.first)));
  for (auto& bag : m_bagsNetState) {
    auto& items = m_bags[bag.first]->items();
    for (std::size_t i = 0; i < bag.second.size(); ++i) bag.second[i].set(itemSafeDescriptor(items[i]));
  }
  m_swapSlotNetState.set(itemSafeDescriptor(m_swapSlot));
  m_trashSlotNetState.set(itemSafeDescriptor(m_trashSlot));
  m_currenciesNetState.set(m_currencies);
  m_customBarGroupNetState.set(m_customBarGroup);
  for (std::size_t i = 0; i < m_customBar.size(0); ++i)
    for (std::size_t j = 0; j < m_customBar.size(1); ++j)
      m_customBarNetState(i, j).set(m_customBar(i, j));
  m_selectedActionBarNetState.set(m_selectedActionBar);
  for (auto& entry : m_essentialNetState) entry.second.set(itemSafeDescriptor(m_essential.value(entry.first)));
  return {};
}

}
