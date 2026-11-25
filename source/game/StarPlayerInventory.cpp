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
import star.array;
import star.ordered_set;
import star.audio;
import star.map;
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
namespace {

constexpr std::size_t slot_capacity = std::size_t{1} + std::numeric_limits<std::uint8_t>::max();

std::expected<std::size_t, StarException> bounded_integer(Json const& object, String const& key, std::size_t minimum, std::size_t maximum) {
  auto value = object.get_double(key);
  if (!value) return std::unexpected(std::move(value.error()));
  // Counts alone admit 256; all other representable values are slot indices.
  if (maximum == slot_capacity && std::bit_cast<std::uint64_t>(*value) == std::bit_cast<std::uint64_t>(256.0))
    return slot_capacity;
  if (auto index = inventory_index(*value); index && *index >= minimum && *index <= maximum)
    return *index;
  return std::unexpected(InventoryException::format("{} must be an integer in [{}, {}]", key, minimum, maximum));
}

// These legacy save fields are not the action-bar tool names.
constexpr std::array<std::pair<EssentialItem, std::string_view>, essential_item_count> essential_save_keys{{
  {EssentialItem::BeamAxe, "beamAxe"}, {EssentialItem::WireTool, "wireTool"},
  {EssentialItem::PaintTool, "paintTool"}, {EssentialItem::InspectionTool, "inspectionTool"}
}};

}

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
    m_custom_bar_group(0), m_equipmentVisibilityMask(0xFFFFFFFF) {}

[[nodiscard]] std::expected<std::shared_ptr<PlayerInventory>, StarException> PlayerInventory::create(Root& root, Json inventoryConfig, Json currenciesConfig) {
  auto inventory = std::shared_ptr<PlayerInventory>(new PlayerInventory(root, std::move(inventoryConfig), std::move(currenciesConfig)));
  auto initialized = inventory->initialize();
  if (!initialized) return std::unexpected(std::move(initialized.error()));
  return inventory;
}

[[nodiscard]] std::expected<void, StarException> PlayerInventory::initialize() {
  if (m_inventoryConfig.is_null()) {
    auto assets = m_root.assets();
    if (!assets) return std::unexpected(std::move(assets.error()));
    auto config = (*assets)->json("/player.config:inventory");
    if (!config) return std::unexpected(std::move(config.error()));
    auto currencies = (*assets)->json("/currencies.config");
    if (!currencies) return std::unexpected(std::move(currencies.error()));
    m_inventoryConfig = std::move(*config);
    m_currenciesConfig = std::move(*currencies);
  }
  auto groups = bounded_integer(m_inventoryConfig, "customBarGroups", 1, slot_capacity);
  if (!groups) return std::unexpected(std::move(groups.error()));
  auto indexes = bounded_integer(m_inventoryConfig, "customBarIndexes", 1, slot_capacity);
  if (!indexes) return std::unexpected(std::move(indexes.error()));
  auto bag_config = m_inventoryConfig.get("itemBags");
  if (!bag_config) return std::unexpected(std::move(bag_config.error()));
  auto bags = bag_config->iterate_object();
  if (!bags) return std::unexpected(std::move(bags.error()));
  struct BagLayout {
    std::reference_wrapper<String const> name;
    std::int64_t priority;
    std::size_t size;
  };
  List<BagLayout> layout;
  for (auto const& [name, config] : *bags) {
    if (is_reserved_bag_name(name))
      return std::unexpected(InventoryException::format("Reserved inventory bag name '{}'", name));
    auto priority = config.get_int("priority", 0);
    if (!priority) return std::unexpected(std::move(priority.error()));
    auto size = bounded_integer(config, "size", 0, slot_capacity);
    if (!size) return std::unexpected(std::move(size.error()));
    layout.push_back({std::cref(name), *priority, *size});
  }
  layout.sort([](BagLayout const& a, BagLayout const& b) { return a.priority < b.priority; });
  for (auto const& bag : layout) {
    m_bags[bag.name.get()] = std::make_shared<ItemBag>(m_root, bag.size);
    m_bagsNetState[bag.name.get()].resize(bag.size);
  }
  auto currencies = m_currenciesConfig.to_object();
  if (!currencies) return std::unexpected(std::move(currencies.error()));
  for (auto const& currency : *currencies) m_currencies[currency.first] = 0;
  auto resized = m_custom_bar.resize(*groups, *indexes);
  if (!resized) return std::unexpected(std::move(resized.error()));

  for (auto const& [slot, name] : equipment_slot_names) {
    auto& element = m_equipmentNetState[slot];
    if (slot > EquipmentSlot::BackCosmetic)
      element.set_compatibility_version(9);
    auto added = add_net_element(element);
    if (!added) return std::unexpected(std::move(added.error()));
  }

  for (auto& bag : m_bagsNetState) {
    for (auto& element : bag.second) {
      auto added = add_net_element(element);
      if (!added) return std::unexpected(std::move(added.error()));
    }
  }
  for (std::reference_wrapper<NetElement> element : {std::ref<NetElement>(m_swapSlotNetState), std::ref<NetElement>(m_trashSlotNetState),
      std::ref<NetElement>(m_currenciesNetState), std::ref<NetElement>(m_custom_bar_group_net_state)}) {
    auto added = add_net_element(element.get());
    if (!added) return std::unexpected(std::move(added.error()));
  }
  auto net_resized = m_custom_bar_net_state.resize(*groups, *indexes);
  if (!net_resized) return std::unexpected(std::move(net_resized.error()));
  for (std::size_t i = 0; i < *groups; ++i) {
    for (std::size_t j = 0; j < *indexes; ++j) {
      auto added = add_net_element(m_custom_bar_net_state(i, j));
      if (!added) return std::unexpected(std::move(added.error()));
    }
  }
  auto selected = add_net_element(m_selected_action_bar_net_state);
  if (!selected) return std::unexpected(std::move(selected.error()));
  for (auto const& [item, name] : essential_item_names) {
    auto added = add_net_element(m_essentialNetState[item]);
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
    if (dest) {
      dest->stack_with(optional_ref(items));
    } else {
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
    auto allowed = itemAllowedInBag(secondItems, std::get<BagSlot>(first).bag_name);
    if (!allowed) return std::unexpected(std::move(allowed.error()));
    if (!*allowed) return false;
  }
  if (std::holds_alternative<BagSlot>(second)) {
    auto allowed = itemAllowedInBag(firstItems, std::get<BagSlot>(second).bag_name);
    if (!allowed) return std::unexpected(std::move(allowed.error()));
    if (!*allowed) return false;
  }
  if (std::holds_alternative<EquipmentSlot>(first) && (item_safe_count(secondItems) > 1 || !itemAllowedAsEquipment(secondItems, std::get<EquipmentSlot>(first))))
    return false;
  if (std::holds_alternative<EquipmentSlot>(second) && (item_safe_count(firstItems) > 1 || !itemAllowedAsEquipment(firstItems, std::get<EquipmentSlot>(second))))
    return false;

  std::swap(firstItems, secondItems);
  swap_custom_bar_links(first, second);

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
    auto const& bs = std::get<BagSlot>(slot);
    auto allowed = itemAllowedInBag(item, bs.bag_name);
    if (!allowed) return std::unexpected(std::move(allowed.error()));
    if (*allowed) {
      auto bag = m_bags.find_ref(bs.bag_name);
      if (!bag) return std::unexpected(ItemException("Invalid inventory bag"));
      auto assigned = bag->get()->set_item(bs.index, item);
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

bool PlayerInventory::slot_valid(InventorySlot const& slot) const {
  if (std::holds_alternative<EquipmentSlot>(slot))
    return std::to_underlying(std::get<EquipmentSlot>(slot)) < equipment_slot_names.size();
  if (std::holds_alternative<BagSlot>(slot)) {
    auto const& bag_slot = std::get<BagSlot>(slot);
    auto bag = m_bags.find_ref(bag_slot.bag_name);
    return bag && bag_slot.index < bag->get()->size();
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
    auto primary = primary_held_item();
    if (!primary) return std::unexpected(std::move(primary.error()));
    if (*primary) {
      (*primary)->stack_with(optional_ref(items));
      if (items->empty()) return std::shared_ptr<Item>();
    }
  }

  // Then, finally the bags
  return addToBags(std::move(items));
}

[[nodiscard]] std::expected<std::shared_ptr<Item>, StarException> PlayerInventory::addToBags(std::shared_ptr<Item> items) {
  if (!items || items->empty())
    return {};

  for (auto const& pair : m_bags) {
    auto allowed = itemAllowedInBag(items, pair.first);
    if (!allowed) return std::unexpected(std::move(allowed.error()));
    if (!*allowed) continue;
    auto stacked = pair.second->stack_items(items);
    if (!stacked) return std::unexpected(std::move(stacked.error()));
    items = std::move(*stacked);
    if (!items) break;

    for (std::size_t i = 0; i < pair.second->size(); ++i) {
      auto slot = std::as_const(*pair.second).at(i);
      if (!slot) return std::unexpected(std::move(slot.error()));
      if (!*slot) {
        auto assigned = pair.second->set_item(i, std::exchange(items, {}));
        if (!assigned) return std::unexpected(std::move(assigned.error()));
        auto added = auto_add_to_custom_bar(BagSlot{pair.first, static_cast<std::uint8_t>(i)});
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

  std::uint64_t can_fit = 0;

  // First, check the equipment slots
  if (is<HeadArmor>(items) && !headArmor())
    ++can_fit;
  if (is<ChestArmor>(items) && !chestArmor())
    ++can_fit;
  if (is<LegsArmor>(items) && !legsArmor())
    ++can_fit;
  if (is<BackArmor>(items) && !backArmor())
    ++can_fit;

  // Then add into bags
  for (auto const& pair : m_bags) {
    auto allowed = itemAllowedInBag(items, pair.first);
    if (!allowed) return std::unexpected(std::move(allowed.error()));
    if (*allowed && can_fit < items->count())
      can_fit += std::min(items->count() - can_fit, pair.second->items_can_fit(optional_ref(items)));
  }

  return std::min(can_fit, items->count());
}

bool PlayerInventory::hasItem(ItemDescriptor const& descriptor, bool exactMatch) const {
  return hasCountOfItem(descriptor, exactMatch) >= descriptor.count();
}

std::uint64_t PlayerInventory::hasCountOfItem(ItemDescriptor const& descriptor, bool exactMatch) const {
  std::uint64_t total = 0;
  auto add = [&](std::uint64_t count) {
    total += std::min(count, std::numeric_limits<std::uint64_t>::max() - total);
  };
  auto count_slot = [&](std::shared_ptr<Item> const& item) {
    if (item && item->matches(descriptor, exactMatch))
      add(item->count());
  };
  count_slot(m_swapSlot);
  count_slot(m_trashSlot);
  for (auto const& slot : m_equipment)
    count_slot(slot.second);
  for (auto const& bag : m_bags)
    add(bag.second->matching_count(descriptor, exactMatch));
  return total;
}

bool PlayerInventory::consumeItems(ItemDescriptor const& descriptor, bool exactMatch) {
  auto remaining = descriptor.count();
  if (!remaining)
    return true;
  if (hasCountOfItem(descriptor, exactMatch) < remaining)
    return false;
  for (auto const& bag : m_bags) {
    auto take = std::min(remaining, bag.second->matching_count(descriptor, exactMatch));
    if (take && !bag.second->consume_items(descriptor.with_count(take), exactMatch))
      return false;
    remaining -= take;
    if (!remaining)
      return true;
  }
  auto consume_slot = [&](std::shared_ptr<Item>& item) {
    if (remaining && item && item->matches(descriptor, exactMatch)) {
      auto take = std::min(remaining, item->count());
      if (item->consume(take))
        remaining -= take;
      normalizeItem(item);
    }
  };
  for (auto& slot : m_equipment)
    consume_slot(slot.second);
  consume_slot(m_swapSlot);
  consume_slot(m_trashSlot);
  return remaining == 0;
}

std::expected<bool, StarException> PlayerInventory::consume_recipe(ItemRecipe const& recipe, std::uint64_t batches) {
  if (!batches)
    return false;
  auto stock = availableItems();
  if (!stock)
    return std::unexpected(std::move(stock.error()));
  if (batches > ItemDatabase::max_craftable_in_bag(*stock, m_currencies, recipe))
    return false;
  // Each product is bounded by the division in max_craftable_in_bag.
  for (auto const& input : recipe.inputs) {
    if (!consumeItems(input.with_count(input.count() * batches), recipe.matchInputParameters))
      return std::unexpected(InventoryException("Recipe inventory changed during consumption"));
  }
  for (auto const& [name, amount] : recipe.currencyInputs) {
    if (amount && !consumeCurrency(name, amount * batches))
      return std::unexpected(InventoryException("Recipe currency changed during consumption"));
  }
  return true;
}

[[nodiscard]] std::expected<ItemDescriptor, StarException> PlayerInventory::takeItems(ItemDescriptor const& descriptor, bool takePartial, bool exactMatch) {
  std::uint64_t hasCount = hasCountOfItem(descriptor, exactMatch);

  if (hasCount >= descriptor.count() || (takePartial && hasCount > 0)) {
    ItemDescriptor consumeDescriptor = descriptor.with_count(std::min(descriptor.count(), hasCount));
    consumeItems(consumeDescriptor, exactMatch);
    return consumeDescriptor;
  }

  return {};
}

[[nodiscard]] std::expected<RecipeStock, StarException> PlayerInventory::availableItems() const {
  RecipeStock stock;
  auto visited = for_each_item([&](InventorySlot const&, std::shared_ptr<Item> const& item) -> std::expected<void, StarException> {
    stock.add(item->descriptor());
    return {};
  });
  if (!visited)
    return std::unexpected(std::move(visited.error()));
  return stock;
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
  auto condensed = entry->get()->condense_stacks();
  if (!condensed) return std::unexpected(std::move(condensed.error()));
  for (std::size_t i = 0; i < m_custom_bar.extent(0); ++i) {
    for (std::size_t j = 0; j < m_custom_bar.extent(1); ++j) {
      auto& link = m_custom_bar(i, j);
      for (auto slotRef : {std::ref(link.primary), std::ref(link.secondary)}) {
        auto& slot = slotRef.get();
        if (slot) {
          if (std::holds_alternative<BagSlot>(*slot) && std::get<BagSlot>(*slot).bag_name == bagType) {
            auto item = std::as_const(*entry->get()).at(std::get<BagSlot>(*slot).index);
            if (!item) return std::unexpected(std::move(item.error()));
            if (!*item) slot.reset();
          }
        }
      }
    }
  }
  return {};
}

[[nodiscard]] std::expected<void, StarException> PlayerInventory::sortBag(String const& bag_type) {
  auto entry = m_bags.find_ref(bag_type);
  if (!entry) return std::unexpected(ItemException("Invalid inventory bag"));
  auto& bag = *entry->get();
  MultiArray<std::pair<std::shared_ptr<Item>, std::shared_ptr<Item>>, 2> saved(m_custom_bar.extents());
  for (std::size_t i = 0; i < m_custom_bar.extent(0); ++i) {
    for (std::size_t j = 0; j < m_custom_bar.extent(1); ++j) {
      auto const& link = m_custom_bar(i, j);
      for (bool primary : {true, false}) {
        auto const& slot = primary ? link.primary : link.secondary;
        if (slot) {
          if (std::holds_alternative<BagSlot>(*slot) && std::get<BagSlot>(*slot).bag_name == bag_type) {
            auto item = std::as_const(bag).at(std::get<BagSlot>(*slot).index);
            if (!item) return std::unexpected(std::move(item.error()));
            (primary ? saved(i, j).first : saved(i, j).second) = std::move(*item);
          }
        }
      }
    }
  }
  auto database = m_root.itemDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  Map<String, ItemType> types;
  auto& items = bag.items();
  for (auto const& item : items) {
    if (item && !types.contains(item->name())) {
      auto length = item->name().size();
      if (!length) return std::unexpected(std::move(length.error()));
      auto type = (*database)->item_type(item->name());
      if (!type) return std::unexpected(std::move(type.error()));
      types[item->name()] = *type;
    }
  }
  items.sort([&types](std::shared_ptr<Item> const& a, std::shared_ptr<Item> const& b) {
    if (a && !b) return true;
    if (!a) return false;
    auto a_type = types.value(a->name());
    auto b_type = types.value(b->name());
    if (a_type != b_type) return a_type < b_type;
    if (a->rarity() != b->rarity()) return a->rarity() > b->rarity();
    if (a->name().utf8() != b->name().utf8()) return a->name().utf8() < b->name().utf8();
    return a->count() > b->count();
  });
  Map<std::shared_ptr<Item>, std::size_t> indexes;
  for (std::size_t i = 0; i < items.size(); ++i)
    if (items[i]) indexes[items[i]] = i;
  for (std::size_t i = 0; i < m_custom_bar.extent(0); ++i) {
    for (std::size_t j = 0; j < m_custom_bar.extent(1); ++j) {
      auto const& old = saved(i, j);
      if (old.first) m_custom_bar(i, j).primary.emplace(BagSlot{bag_type, static_cast<std::uint8_t>(indexes.value(old.first))});
      if (old.second) m_custom_bar(i, j).secondary.emplace(BagSlot{bag_type, static_cast<std::uint8_t>(indexes.value(old.second))});
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
      if (item_safe_count(m_swapSlot) <= 1) {
        std::swap(m_swapSlot, equipSlot);
        swap_custom_bar_links(SwapSlot(), slot);
      } else if (item_safe_count(equipSlot) == 0) {
        auto taken = m_swapSlot->take(1);
        if (!taken) return std::unexpected(std::move(taken.error()));
        equipSlot = std::move(*taken);
      }
    }
  } else if (std::holds_alternative<TrashSlot>(slot)) {
    normalizeItem(m_trashSlot);
    std::swap(m_swapSlot, m_trashSlot);
    swap_custom_bar_links(SwapSlot(), slot);
  } else if (std::holds_alternative<BagSlot>(slot)) {
    auto const& bs = std::get<BagSlot>(slot);
    auto allowed = itemAllowedInBag(m_swapSlot, bs.bag_name);
    if (!allowed) return std::unexpected(std::move(allowed.error()));
    if (*allowed) {
      auto bag = m_bags.find_ref(bs.bag_name);
      if (!bag) return std::unexpected(ItemException("Invalid inventory bag"));
      auto swapped = bag->get()->swap_items(bs.index, m_swapSlot);
      if (!swapped) return std::unexpected(std::move(swapped.error()));
      m_swapSlot = std::move(*swapped);
      swap_custom_bar_links(SwapSlot(), slot);
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
  auto trySlot = [&](InventorySlot const& slot) -> std::expected<void, StarException> {
    if (!m_swapSlot) return {};
    auto stacked = stackWith(slot, m_swapSlot);
    if (!stacked) return std::unexpected(std::move(stacked.error()));
    m_swapSlot = std::move(*stacked);
    if (!m_swapSlot) swap_custom_bar_links(SwapSlot(), slot);
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
    for (std::size_t i = 0; i < bag.second->size() && m_swapSlot; ++i) {
      auto allowed = itemAllowedInBag(m_swapSlot, bag.first);
      if (!allowed) return std::unexpected(std::move(allowed.error()));
      if (!*allowed) break;
      auto tried = trySlot(BagSlot{bag.first, static_cast<std::uint8_t>(i)});
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
    auto added = auto_add_to_custom_bar(SwapSlot());
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
  auto maximum = currency->get_uint("playerMax", std::numeric_limits<std::uint64_t>::max());
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

[[nodiscard]] std::expected<std::optional<InventorySlot>, StarException> PlayerInventory::custom_bar_primary_slot(std::size_t index) const {
  auto link = m_custom_bar.at(m_custom_bar_group, index);
  if (!link) return std::unexpected(std::move(link.error()));
  return link->get().primary;
}

[[nodiscard]] std::expected<std::optional<InventorySlot>, StarException> PlayerInventory::custom_bar_secondary_slot(std::size_t index) const {
  auto link = m_custom_bar.at(m_custom_bar_group, index);
  if (!link) return std::unexpected(std::move(link.error()));
  return link->get().secondary;
}

[[nodiscard]] std::expected<void, StarException> PlayerInventory::set_custom_bar_primary_slot(std::size_t index, std::optional<InventorySlot> slot) {
  if (slot) {
    auto item = itemsAt(*slot);
    if (!item) return std::unexpected(std::move(item.error()));
    if (!*item) slot.reset();
  }
  auto link = m_custom_bar.at(m_custom_bar_group, index);
  if (!link) return std::unexpected(std::move(link.error()));
  auto& cbl = link->get();
  if (slot && cbl.secondary == slot) std::swap(cbl.primary, cbl.secondary);
  else cbl.primary = std::move(slot);
  return {};
}

[[nodiscard]] std::expected<void, StarException> PlayerInventory::set_custom_bar_secondary_slot(std::size_t index, std::optional<InventorySlot> slot) {
  auto link = m_custom_bar.at(m_custom_bar_group, index);
  if (!link) return std::unexpected(std::move(link.error()));
  auto& cbl = link->get();
  if (slot) {
    auto item = itemsAt(*slot);
    if (!item) return std::unexpected(std::move(item.error()));
    if (!*item || item_safe_two_handed(*item)) slot.reset();
  }
  bool primaryTwoHanded = false;
  if (cbl.primary) {
    auto item = itemsAt(*cbl.primary);
    if (!item) return std::unexpected(std::move(item.error()));
    primaryTwoHanded = item_safe_two_handed(*item);
  }
  if (cbl.primary && cbl.primary == slot && !primaryTwoHanded) std::swap(cbl.primary, cbl.secondary);
  else {
    cbl.secondary = std::move(slot);
    if (primaryTwoHanded) cbl.primary.reset();
  }
  return {};
}

std::expected<void, StarException> PlayerInventory::link_custom_bar_swap(std::size_t index, bool primary) {
  if (auto item = swapSlotItem()) {
    auto linked = primary || item->two_handed()
      ? set_custom_bar_primary_slot(index, InventorySlot(SwapSlot()))
      : set_custom_bar_secondary_slot(index, InventorySlot(SwapSlot()));
    if (!linked) return std::unexpected(std::move(linked.error()));
    return clearSwap().transform([](bool) {});
  }
  if (index >= custom_bar_indexes()) return std::unexpected(InventoryException("Invalid custom bar index"));
  return select_action_bar_location(static_cast<CustomBarIndex>(index));
}

std::expected<void, StarException> PlayerInventory::clear_custom_bar_link(std::size_t index, bool primary) {
  auto link = m_custom_bar.at(m_custom_bar_group, index);
  if (!link) return std::unexpected(std::move(link.error()));
  if (!primary && link->get().primary) {
    auto item = itemsAt(*link->get().primary);
    if (!item) return std::unexpected(std::move(item.error()));
    primary = item_safe_two_handed(*item);
  }
  return primary ? set_custom_bar_primary_slot(index, {}) : set_custom_bar_secondary_slot(index, {});
}

[[nodiscard]] std::expected<void, StarException> PlayerInventory::add_to_custom_bar(InventorySlot slot) {
  if (!slot_valid(slot)) return std::unexpected(InventoryException("Invalid custom bar link"));
  for (std::size_t j = 0; j < m_custom_bar.extent(1); ++j) {
    auto result = m_custom_bar.at(m_custom_bar_group, j);
    if (!result) return std::unexpected(std::move(result.error()));
    auto& cbl = result->get();
    if (!cbl.primary && !cbl.secondary) {
      cbl.primary.emplace(std::move(slot));
      break;
    }
  }
  return {};
}

std::uint8_t PlayerInventory::custom_bar_group() const {
  return m_custom_bar_group;
}

std::expected<void, StarException> PlayerInventory::set_custom_bar_group(std::uint64_t group) {
  if (group >= custom_bar_groups()) return std::unexpected(InventoryException("Invalid custom bar group"));
  m_custom_bar_group = static_cast<std::uint8_t>(group);
  return {};
}

std::size_t PlayerInventory::custom_bar_groups() const {
  return m_custom_bar.extent(0);
}

std::size_t PlayerInventory::custom_bar_indexes() const {
  return m_custom_bar.extent(1);
}

SelectedActionBarLocation PlayerInventory::selected_action_bar_location() const {
  return m_selected_action_bar;
}

bool PlayerInventory::action_bar_location_valid(SelectedActionBarLocation const& location) const {
  if (std::holds_alternative<CustomBarIndex>(location))
    return std::get<CustomBarIndex>(location) < custom_bar_indexes();
  if (std::holds_alternative<EssentialItem>(location))
    return std::to_underlying(std::get<EssentialItem>(location)) < essential_item_count;
  return true;
}

std::expected<void, StarException> PlayerInventory::select_action_bar_location(SelectedActionBarLocation location) {
  if (!action_bar_location_valid(location)) return std::unexpected(InventoryException("Invalid action bar location"));
  m_selected_action_bar = location;
  return {};
}

[[nodiscard]] std::expected<std::shared_ptr<Item>, StarException> PlayerInventory::primary_held_item() const {
  if (m_swapSlot && !m_swapSlot->empty())
    return m_swapSlot;

  if (std::holds_alternative<EssentialItem>(m_selected_action_bar))
    return m_essential.value(std::get<EssentialItem>(m_selected_action_bar));

  if (std::holds_alternative<CustomBarIndex>(m_selected_action_bar)) {
    auto link = m_custom_bar.at(m_custom_bar_group, std::get<CustomBarIndex>(m_selected_action_bar));
    if (!link) return std::unexpected(std::move(link.error()));
    if (link->get().primary) return retrieve(*link->get().primary);
  }

  return {};
}

std::expected<std::optional<std::reference_wrapper<InventorySlot const>>, StarException> PlayerInventory::secondary_held_link() const {
  if ((m_swapSlot && !m_swapSlot->empty()) || !std::holds_alternative<CustomBarIndex>(m_selected_action_bar))
    return {};
  auto link = m_custom_bar.at(m_custom_bar_group, std::get<CustomBarIndex>(m_selected_action_bar));
  if (!link) return std::unexpected(std::move(link.error()));
  auto const& [primary, secondary] = link->get();
  if (primary) {
    auto item = retrieve(*primary);
    if (!item) return std::unexpected(std::move(item.error()));
    if (item_safe_two_handed(*item)) return {};
  }
  if (secondary) return std::cref(*secondary);
  return {};
}

[[nodiscard]] std::expected<std::shared_ptr<Item>, StarException> PlayerInventory::secondary_held_item() const {
  auto link = secondary_held_link();
  if (!link) return std::unexpected(std::move(link.error()));
  if (*link) return retrieve((*link)->get());
  return {};
}

[[nodiscard]] std::expected<std::optional<InventorySlot>, StarException> PlayerInventory::primary_held_slot() const {
  if (m_swapSlot && !m_swapSlot->empty())
    return std::optional<InventorySlot>(SwapSlot());
  if (std::holds_alternative<CustomBarIndex>(m_selected_action_bar))
    return custom_bar_primary_slot(std::get<CustomBarIndex>(m_selected_action_bar));
  return {};
}

[[nodiscard]] std::expected<std::optional<InventorySlot>, StarException> PlayerInventory::secondary_held_slot() const {
  auto link = secondary_held_link();
  if (!link) return std::unexpected(std::move(link.error()));
  if (*link) return (*link)->get();
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
    auto loaded = (*database)->disk_load(*json);
    if (!loaded) return std::unexpected(std::move(loaded.error()));
    item = std::move(*loaded);
    normalizeItem(item);
    return {};
  };
  for (auto const& slot : equipment_slot_names) {
    auto loaded = loadItem(Star::format("{}Slot", slot.second), m_equipment[slot.first], true);
    if (!loaded) return std::unexpected(std::move(loaded.error()));
  }
  auto bags = store.get_object("itemBags");
  if (!bags) return std::unexpected(std::move(bags.error()));
  m_inventoryLoadOverflow.clear();
  for (auto const& entry : *bags) {
    auto loaded = ItemBag::load_store(m_root, entry.second);
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
  auto currencies = store.get_object("currencies");
  if (!currencies) return std::unexpected(std::move(currencies.error()));
  m_currencies.clear();
  for (auto const& entry : *currencies) {
    auto count = entry.second.to_uint();
    if (!count) return std::unexpected(std::move(count.error()));
    m_currencies[entry.first] = *count;
  }
  auto group = bounded_integer(store, "customBarGroup", 0, slot_capacity - 1);
  if (!group) return std::unexpected(std::move(group.error()));
  // The saved group may no longer exist after a layout change.
  m_custom_bar_group = *group < custom_bar_groups() ? static_cast<std::uint8_t>(*group) : 0;
  auto bar = store.get("customBar");
  if (!bar) return std::unexpected(std::move(bar.error()));
  for (std::size_t i = 0; i < m_custom_bar.extent(0); ++i) {
    auto row = bar->get(i, JsonArray());
    if (!row) return std::unexpected(std::move(row.error()));
    for (std::size_t j = 0; j < m_custom_bar.extent(1); ++j) {
      auto links = row->get(j, JsonArray());
      if (!links) return std::unexpected(std::move(links.error()));
      for (std::size_t hand = 0; hand < 2; ++hand) {
        auto json = links->get(hand, {});
        if (!json) return std::unexpected(std::move(json.error()));
        std::optional<InventorySlot> link;
        if (!json->is_null()) {
          auto parsed = json_to_inventory_slot(*json);
          if (!parsed) return std::unexpected(std::move(parsed.error()));
          if (slot_valid(*parsed)) link = std::move(*parsed);
        }
        (hand == 0 ? m_custom_bar(i, j).primary : m_custom_bar(i, j).secondary) = std::move(link);
      }
    }
  }
  auto selected = store.get("selectedActionBar");
  if (!selected) return std::unexpected(std::move(selected.error()));
  auto location = json_to_selected_action_bar_location(*selected);
  if (!location) return std::unexpected(std::move(location.error()));
  m_selected_action_bar = action_bar_location_valid(*location) ? *location : SelectedActionBarLocation{};
  m_essential.clear();
  for (auto const& [item, key] : essential_save_keys) {
    auto loaded = loadItem(key, m_essential[item]);
    if (!loaded) return std::unexpected(std::move(loaded.error()));
  }
  auto visibility = store.opt_uint("equipmentVisibilityMask");
  if (!visibility) return std::unexpected(std::move(visibility.error()));
  m_equipmentVisibilityMask = static_cast<unsigned>(visibility->value_or(0xFFFFFFFF));
  return {};
}

[[nodiscard]] std::expected<Json, StarException> PlayerInventory::store() const {
  auto database = m_root.itemDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  JsonArray bar;
  for (std::size_t i = 0; i < m_custom_bar.extent(0); ++i) {
    JsonArray row;
    for (std::size_t j = 0; j < m_custom_bar.extent(1); ++j) {
      auto const& link = m_custom_bar(i, j);
      JsonArray hands;
      for (auto slotRef : {std::cref(link.primary), std::cref(link.secondary)}) {
        auto const& slot = slotRef.get();
        Json json;
        if (slot) {
          auto encoded = json_from_inventory_slot(*slot);
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
    auto encoded = bag.second->disk_store();
    if (!encoded) return std::unexpected(std::move(encoded.error()));
    bags[bag.first] = std::move(*encoded);
  }
  JsonObject currencies;
  for (auto const& currency : m_currencies) currencies[currency.first] = currency.second;
  auto selected = json_from_selected_action_bar_location(m_selected_action_bar);
  if (!selected) return std::unexpected(std::move(selected.error()));
  JsonObject data{{"itemBags", std::move(bags)}, {"currencies", std::move(currencies)},
    {"customBarGroup", m_custom_bar_group}, {"customBar", std::move(bar)},
    {"selectedActionBar", std::move(*selected)}, {"equipmentVisibilityMask", m_equipmentVisibilityMask}};
  auto saveItem = [&](String const& key, std::shared_ptr<Item> const& item) -> std::expected<void, StarException> {
    auto encoded = (*database)->disk_store(item);
    if (!encoded) return std::unexpected(std::move(encoded.error()));
    data[key] = std::move(*encoded);
    return {};
  };
  auto swap = saveItem("swapSlot", m_swapSlot);
  if (!swap) return std::unexpected(std::move(swap.error()));
  auto trash = saveItem("trashSlot", m_trashSlot);
  if (!trash) return std::unexpected(std::move(trash.error()));
  for (auto const& [item, key] : essential_save_keys) {
    auto saved = saveItem(key, m_essential.value(item));
    if (!saved) return std::unexpected(std::move(saved.error()));
  }
  for (auto const& equipment : m_equipment) {
    if (equipment.first <= EquipmentSlot::BackCosmetic || equipment.second) {
      auto name = enum_name<equipment_slot_names>(equipment.first);
      if (!name) return std::unexpected(std::move(name.error()));
      auto saved = saveItem(Star::format("{}Slot", *name), equipment.second);
      if (!saved) return std::unexpected(std::move(saved.error()));
    }
  }
  return data;
}

[[nodiscard]] std::expected<void, StarException> PlayerInventory::for_each_item(std::function<std::expected<void, StarException>(InventorySlot const&, std::shared_ptr<Item>&)> function) {
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
    std::optional<InventorySlot> position;
    for (std::size_t i = 0; i < bag.second->size(); ++i) {
      auto item = bag.second->at(i);
      if (!item) return std::unexpected(std::move(item.error()));
      auto& current = item->get(); // Mutable at() already normalized this slot.
      if (!current) continue;
      if (!position) position.emplace(std::in_place_type<BagSlot>, bag.first, static_cast<std::uint8_t>(i));
      else std::get<BagSlot>(*position).index = static_cast<std::uint8_t>(i);
      auto result = function(*position, current);
      normalizeItem(current);
      if (!result) return std::unexpected(std::move(result.error()));
    }
  }
  auto swap = visit(SwapSlot(), m_swapSlot);
  if (!swap) return std::unexpected(std::move(swap.error()));
  return visit(TrashSlot(), m_trashSlot);
}

[[nodiscard]] std::expected<void, StarException> PlayerInventory::for_each_item(std::function<std::expected<void, StarException>(InventorySlot const&, std::shared_ptr<Item> const&)> function) const {
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
    std::optional<InventorySlot> position;
    for (std::size_t i = 0; i < contents.size(); ++i) {
      auto item = contents.at(i);
      if (!item) return std::unexpected(std::move(item.error()));
      if (!*item) continue;
      if (!position) position.emplace(std::in_place_type<BagSlot>, bag.first, static_cast<std::uint8_t>(i));
      else std::get<BagSlot>(*position).index = static_cast<std::uint8_t>(i);
      auto result = function(*position, *item);
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
    for_each_item([&](InventorySlot const&, std::shared_ptr<Item> const& item) -> std::expected<void, StarException> {
      items.push_back(item);
      return {};
    });
  if (!result) return std::unexpected(std::move(result.error()));
  return items;
}

[[nodiscard]] std::expected<Map<String, std::uint64_t>, StarException> PlayerInventory::itemSummary() const {
  Map<String, std::uint64_t> summary;
  auto result = for_each_item([&](InventorySlot const&, std::shared_ptr<Item> const& item) -> std::expected<void, StarException> {
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
  for (std::size_t i = 0; i < m_custom_bar.extent(0); ++i) {
    for (std::size_t j = 0; j < m_custom_bar.extent(1); ++j) {
      auto& link = m_custom_bar(i, j);
      std::shared_ptr<Item> primary, secondary;
      if (link.primary) {
        auto item = retrieve(*link.primary);
        if (!item) return std::unexpected(std::move(item.error()));
        primary = *item;
      }
      if (link.secondary) {
        auto item = retrieve(*link.secondary);
        if (!item) return std::unexpected(std::move(item.error()));
        secondary = *item;
      }
      if (!primary) link.primary.reset();
      if (!secondary || item_safe_two_handed(primary) || item_safe_two_handed(secondary)) link.secondary.reset();
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

[[nodiscard]] std::expected<void, StarException> PlayerInventory::net_store(DataStream& ds, NetCompatibilityRules rules) const {
  if (m_player && !m_player->get().isMaster() && !rules.is_admin()) {
    auto blank = blankInventory();
    if (!blank) return std::unexpected(std::move(blank.error()));
    return blank->get().net_store(ds, rules);
  }
  return NetElementSyncGroup::net_store(ds, rules);
}

[[nodiscard]] std::expected<bool, StarException> PlayerInventory::write_net_delta(DataStream& ds, std::uint64_t version, NetCompatibilityRules rules) const {
  if (m_player && !m_player->get().isMaster() && !rules.is_admin()) {
    auto blank = blankInventory();
    if (!blank) return std::unexpected(std::move(blank.error()));
    return blank->get().write_net_delta(ds, version, rules);
  }
  return NetElementSyncGroup::write_net_delta(ds, version, rules);
}

[[nodiscard]] std::expected<bool, StarException> PlayerInventory::checkInventoryFilter(std::shared_ptr<Item> const& items, String const& filterName) const {
  Json filterConfig;
  auto filters = items->instance_value("inventoryFilters");
  if (!filters) return std::unexpected(std::move(filters.error()));
  if (filters->is_type(Json::Type::Object)) {
    auto selected = filters->opt(filterName);
    if (!selected) return std::unexpected(std::move(selected.error()));
    filterConfig = selected->value_or(Json{});
    if (!filterConfig.is_type(Json::Type::Object)) {
      auto fallback = filters->opt("default");
      if (!fallback) return std::unexpected(std::move(fallback.error()));
      filterConfig = fallback->value_or(Json{});
    }
  }
  if (!filterConfig.is_type(Json::Type::Object)) {
    auto assets = m_root.assets();
    if (!assets) return std::unexpected(std::move(assets.error()));
    auto config = (*assets)->json("/player.config:inventoryFilters");
    if (!config) return std::unexpected(std::move(config.error()));
    auto selected = config->opt(filterName);
    if (!selected) return std::unexpected(std::move(selected.error()));
    filterConfig = selected->value_or(Json{});
    if (!filterConfig.is_type(Json::Type::Object)) {
      auto fallback = config->get("default", JsonObject());
      if (!fallback) return std::unexpected(std::move(fallback.error()));
      filterConfig = std::move(*fallback);
    }
  }
  auto database = m_root.itemDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  auto type = (*database)->item_type(items->name());
  if (!type) return std::unexpected(std::move(type.error()));
  auto typeName = enum_name<item_type_names>(*type);
  if (!typeName) return std::unexpected(std::move(typeName.error()));
  for (auto const& entry : {std::pair<String, bool>{"typeWhitelist", false}, {"typeBlacklist", true}}) {
    auto present = filterConfig.contains(entry.first);
    if (!present) return std::unexpected(std::move(present.error()));
    if (*present) {
      auto list = filterConfig.get_array(entry.first);
      if (!list) return std::unexpected(std::move(list.error()));
      if (list->contains(Json(String(*typeName))) == entry.second)
        return false;
    }
  }
  auto tags = (*database)->item_tags(items->name());
  if (!tags) return std::unexpected(std::move(tags.error()));
  for (auto const& entry : {std::pair<String, bool>{"tagWhitelist", false}, {"tagBlacklist", true}}) {
    auto present = filterConfig.contains(entry.first);
    if (!present) return std::unexpected(std::move(present.error()));
    if (*present) {
      auto list = filterConfig.get_array(entry.first);
      if (!list) return std::unexpected(std::move(list.error()));
      bool matched = false;
      for (auto const& value : *list) {
        auto tag = value.to_string();
        if (!tag) return std::unexpected(std::move(tag.error()));
        if (tags->contains(*tag)) matched = true;
      }
      if (matched == entry.second) return false;
    }
  }
  for (auto const& entry : {std::pair<String, bool>{"categoryWhitelist", false}, {"categoryBlacklist", true}}) {
    auto list = filterConfig.opt_array(entry.first);
    if (!list) return std::unexpected(std::move(list.error()));
    if (*list) {
      auto categories = json_to_string_set(**list);
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
    if (auto bag = m_bags.find_ref(bs.bag_name))
      return std::as_const(*bag->get()).at(bs.index);
  } else {
    auto const& item = std::holds_alternative<SwapSlot>(slot) ? m_swapSlot : m_trashSlot;
    return item && !item->empty() ? item : std::shared_ptr<Item>();
  }

  auto json = json_from_inventory_slot(slot);
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
      auto bag = m_bags.find_ref(position.bag_name);
      if (!bag) {
        auto json = json_from_inventory_slot(slot);
        if (!json) return std::unexpected(std::move(json.error()));
        auto diagnostic = json->repr();
        if (!diagnostic) return std::unexpected(std::move(diagnostic.error()));
        return std::unexpected(ItemException::format("Invalid inventory slot {}", *diagnostic));
      }
      return bag->get()->at(position.index);
    }
    return std::ref(std::holds_alternative<SwapSlot>(slot) ? m_swapSlot : m_trashSlot);
  }();
  if (!item) return std::unexpected(std::move(item.error()));
  normalizeItem(item->get());
  return item;
}

void PlayerInventory::swap_custom_bar_links(InventorySlot const& a, InventorySlot const& b) {
  m_custom_bar.for_each([&](Array2S const&, CustomBarLink& p) {
      if (p.primary == a)
        p.primary = b;
      else if (p.primary == b)
        p.primary = a;

      if (p.secondary == a)
        p.secondary = b;
      else if (p.secondary == b)
        p.secondary = a;
    });
}

[[nodiscard]] std::expected<void, StarException> PlayerInventory::auto_add_to_custom_bar(InventorySlot slot) {
  auto configuration = m_root.configuration();
  if (!configuration) return std::unexpected(std::move(configuration.error()));
  auto config = (*configuration)->get_path("inventory.pickupToActionBar");
  if (!config) return std::unexpected(std::move(config.error()));
  auto enabled = config->to_bool();
  if (!enabled) return std::unexpected(std::move(enabled.error()));
  if (!*enabled) return {};
  auto items = itemsAt(slot);
  if (!items) return std::unexpected(std::move(items.error()));
  if (*items && !(*items)->empty()) {
    auto allowed = checkInventoryFilter(*items, "autoAddToCustomBar");
    if (!allowed) return std::unexpected(std::move(allowed.error()));
    if (*allowed) return add_to_custom_bar(std::move(slot));
  }
  return {};
}

[[nodiscard]] std::expected<void, StarException> PlayerInventory::net_elements_need_load(bool) {
  auto group = m_custom_bar_group_net_state.get();
  if (group >= custom_bar_groups()) return std::unexpected(InventoryException("Invalid network custom bar group"));
  auto const& selected = m_selected_action_bar_net_state.get();
  if (!action_bar_location_valid(selected)) return std::unexpected(InventoryException("Invalid network action bar location"));
  for (std::size_t i = 0; i < custom_bar_groups(); ++i) {
    for (std::size_t j = 0; j < custom_bar_indexes(); ++j) {
      auto const& [primary, secondary] = m_custom_bar_net_state(i, j).get();
      if ((primary && !slot_valid(*primary)) || (secondary && !slot_valid(*secondary)))
        return std::unexpected(InventoryException("Invalid network custom bar link"));
    }
  }
  auto database = m_root.itemDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  auto load = [&](NetElementData<ItemDescriptor>& state, std::shared_ptr<Item>& item) -> std::expected<void, StarException> {
    if (state.pull_updated()) {
      auto result = (*database)->load_item(state.get(), item);
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
  m_custom_bar_group = static_cast<std::uint8_t>(group);
  for (std::size_t i = 0; i < m_custom_bar.extent(0); ++i)
    for (std::size_t j = 0; j < m_custom_bar.extent(1); ++j)
      m_custom_bar(i, j) = m_custom_bar_net_state(i, j).get();
  m_selected_action_bar = selected;
  for (auto& entry : m_essentialNetState) {
    auto result = load(entry.second, m_essential[entry.first]);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  return cleanup();
}

[[nodiscard]] std::expected<void, StarException> PlayerInventory::net_elements_need_store() {
  auto cleaned = cleanup();
  if (!cleaned) return std::unexpected(std::move(cleaned.error()));
  for (auto& entry : m_equipmentNetState) entry.second.set(item_safe_descriptor(m_equipment.value(entry.first)));
  for (auto& bag : m_bagsNetState) {
    auto& items = m_bags[bag.first]->items();
    for (std::size_t i = 0; i < bag.second.size(); ++i) bag.second[i].set(item_safe_descriptor(items[i]));
  }
  m_swapSlotNetState.set(item_safe_descriptor(m_swapSlot));
  m_trashSlotNetState.set(item_safe_descriptor(m_trashSlot));
  m_currenciesNetState.set(m_currencies);
  m_custom_bar_group_net_state.set(m_custom_bar_group);
  for (std::size_t i = 0; i < m_custom_bar.extent(0); ++i)
    for (std::size_t j = 0; j < m_custom_bar.extent(1); ++j)
      m_custom_bar_net_state(i, j).set(m_custom_bar(i, j));
  m_selected_action_bar_net_state.set(m_selected_action_bar);
  for (auto& entry : m_essentialNetState) entry.second.set(item_safe_descriptor(m_essential.value(entry.first)));
  return {};
}

}
