import star.memory;
import star.optional;
import star.json_extra;
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
import std;
import star.format;
import star.version;

import star.drawable;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.item;
import star.item_descriptor;
import star.item_recipe;
import star.item_descriptor;
import star.asset_source;
import star.assets;
import star.configuration;
import star.root;


import star.item_database;
import star.item_bag;
import star.versioning_database;

namespace Star {

ItemBag::ItemBag(Root& root, std::size_t size) : m_root(root) {
  m_items.resize(size);
}

std::expected<ItemBag, StarException> ItemBag::fromJson(Root& root, Json const& store) {
  auto itemDatabase = root.itemDatabase();
  if (!itemDatabase)
    return std::unexpected(std::move(itemDatabase.error()));
  auto entries = store.iterateArray();
  if (!entries)
    return std::unexpected(std::move(entries.error()));
  ItemBag res(root, 0);
  for (auto const& entry : *entries) {
    auto item = (*itemDatabase)->fromJson(entry);
    if (!item)
      return std::unexpected(std::move(item.error()));
    res.m_items.push_back(std::move(*item));
  }
  return res;
}

std::expected<ItemBag, StarException> ItemBag::loadStore(Root& root, Json const& store) {
  auto itemDatabase = root.itemDatabase();
  if (!itemDatabase)
    return std::unexpected(std::move(itemDatabase.error()));
  auto entries = store.iterateArray();
  if (!entries)
    return std::unexpected(std::move(entries.error()));
  ItemBag res(root, 0);
  for (auto const& entry : *entries) {
    auto item = (*itemDatabase)->diskLoad(entry);
    if (!item)
      return std::unexpected(std::move(item.error()));
    res.m_items.push_back(std::move(*item));
  }
  return res;
}

std::expected<Json, StarException> ItemBag::toJson() const {
  JsonArray entries;
  for (auto const& item : m_items)
    entries.push_back(item ? item->descriptor().toJson() : Json());
  return Json(std::move(entries));
}

std::expected<Json, StarException> ItemBag::diskStore() const {
  auto versioningDatabase = m_root.get().versioningDatabase();
  if (!versioningDatabase)
    return std::unexpected(std::move(versioningDatabase.error()));
  JsonArray entries;
  for (auto const& item : m_items) {
    if (!item) {
      entries.push_back(Json());
      continue;
    }
    auto stored = item->descriptor().diskStore(**versioningDatabase);
    if (!stored)
      return std::unexpected(std::move(stored.error()));
    entries.push_back(std::move(*stored));
  }
  return Json(std::move(entries));
}

std::size_t ItemBag::size() const {
  return m_items.size();
}

std::expected<List<std::shared_ptr<Item>>, StarException> ItemBag::resize(std::size_t size) {
  List<std::shared_ptr<Item>> lost;
  while (m_items.size() > size) {
    auto lastItem = std::move(m_items[m_items.size() - 1]);
    m_items.resize(m_items.size() - 1);
    auto remaining = addItems(std::move(lastItem));
    if (!remaining)
      return std::unexpected(std::move(remaining.error()));
    if (*remaining && !(*remaining)->empty())
      lost.push_back(std::move(*remaining));
  }
  m_items.resize(size);
  return lost;
}

void ItemBag::clearItems() {
  std::size_t oldSize = m_items.size();
  m_items.clear();
  m_items.resize(oldSize);
}

bool ItemBag::cleanup() const {
  bool cleanupDone = false;
  for (auto& items : m_items) {
    if (items && items->empty()) {
      cleanupDone = true;
      items = {};
    }
  }

  return cleanupDone;
}

List<std::shared_ptr<Item>>& ItemBag::items() {
  // When returning the entire item collection, need to make sure that there
  // are no empty items before returning.
  cleanup();

  return m_items;
}

List<std::shared_ptr<Item>> const& ItemBag::items() const {
  cleanup();
  return m_items;
}

std::expected<std::shared_ptr<Item>, StarException> ItemBag::at(std::size_t i) const {
  auto item = m_items.at(i);
  if (!item)
    return std::unexpected(std::move(item.error()));
  return item->get() && !item->get()->empty() ? item->get() : std::shared_ptr<Item>();
}

std::expected<std::reference_wrapper<std::shared_ptr<Item>>, StarException> ItemBag::at(std::size_t i) {
  auto item = m_items.at(i);
  if (!item)
    return std::unexpected(std::move(item.error()));
  if (item->get() && item->get()->empty())
    item->get() = {};
  return *item;
}

List<std::shared_ptr<Item>> ItemBag::takeAll() {
  cleanup();
  List<std::shared_ptr<Item>> taken;
  for (auto& item : m_items) {
    if (item)
      taken.push_back(std::move(item));
  }
  return taken;
}

std::expected<void, StarException> ItemBag::setItem(std::size_t pos, std::shared_ptr<Item> item) {
  auto storedItem = at(pos);
  if (!storedItem)
    return std::unexpected(std::move(storedItem.error()));
  storedItem->get() = std::move(item);
  return {};
}

std::expected<std::shared_ptr<Item>, StarException> ItemBag::putItems(std::size_t pos, std::shared_ptr<Item> items) {
  if (!items || items->empty())
    return std::shared_ptr<Item>();
  auto slot = at(pos);
  if (!slot)
    return std::unexpected(std::move(slot.error()));
  auto& storedItem = slot->get();
  if (storedItem) {
    storedItem->stackWith(optionalRef(items));
    if (!items->empty())
      return items;
  } else {
    storedItem = std::move(items);
  }
  return std::shared_ptr<Item>();
}

std::expected<std::shared_ptr<Item>, StarException> ItemBag::takeItems(std::size_t pos, std::uint64_t count) {
  auto slot = at(pos);
  if (!slot)
    return std::unexpected(std::move(slot.error()));
  auto& storedItem = slot->get();
  if (!storedItem)
    return std::shared_ptr<Item>();
  auto taken = storedItem->take(count);
  if (!taken)
    return std::unexpected(std::move(taken.error()));
  if (storedItem->empty())
    storedItem.reset();
  return taken;
}

std::expected<std::shared_ptr<Item>, StarException> ItemBag::swapItems(std::size_t pos, std::shared_ptr<Item> items, bool tryCombine) {
  auto slot = at(pos);
  if (!slot)
    return std::unexpected(std::move(slot.error()));
  auto& storedItem = slot->get();

  auto swapItems = std::move(items);
  if (!swapItems || swapItems->empty()) {
    // If we are passed in nothing, simply return what's there, if anything.
    swapItems = storedItem;
    storedItem = {};
  } else if (storedItem) {
    // If something is there, try to stack with it first.  If we can't stack,
    // then swap.
    if (!tryCombine || !storedItem->stackWith(optionalRef(swapItems)))
      std::swap(storedItem, swapItems);
  } else {
    // Otherwise just place the given items in the slot.
    storedItem = swapItems;
    swapItems = {};
  }

  return swapItems;
}

std::expected<bool, StarException> ItemBag::consumeItems(std::size_t pos, std::uint64_t count) {
  auto slot = at(pos);
  if (!slot)
    return std::unexpected(std::move(slot.error()));
  auto& storedItem = slot->get();
  bool consumed = false;
  if (storedItem) {
    consumed = storedItem->consume(count);
    if (storedItem->empty())
      storedItem.reset();
  }
  return consumed;
}

bool ItemBag::consumeItems(ItemDescriptor const& descriptor, bool exactMatch) {
  std::uint64_t countLeft = descriptor.count();
  List<std::pair<std::size_t, std::uint64_t>> consumeLocations;
  for (std::size_t i = 0; i < m_items.size(); ++i) {
    auto& storedItem = m_items[i];
    if (storedItem && storedItem->empty())
      storedItem.reset();
    if (storedItem && storedItem->matches(descriptor, exactMatch)) {
      std::uint64_t count = storedItem->count();
      std::uint64_t take = std::min(count, countLeft);
      consumeLocations.push_back({i, take});
      countLeft -= take;
      if (countLeft == 0)
        break;
    }
  }

  // Only consume any if we can consume them all
  if (countLeft > 0)
    return false;

  for (auto const& loc : consumeLocations) {
    auto& storedItem = m_items[loc.first];
    storedItem->consume(loc.second);
    if (storedItem->empty())
      storedItem.reset();
  }

  return true;
}

std::uint64_t ItemBag::available(ItemDescriptor const& descriptor, bool exactMatch) const {
  std::uint64_t count = 0;
  for (auto const& items : m_items) {
    if (items && items->matches(descriptor, exactMatch))
      count += items->count();
  }

  return count / descriptor.count();
}

std::uint64_t ItemBag::itemsCanFit(std::optional<std::reference_wrapper<Item const>> items) const {
  auto itemsFit = itemsFitWhere(items);
  return items ? items->get().count() - itemsFit.leftover : 0;
}

std::uint64_t ItemBag::itemsCanStack(std::optional<std::reference_wrapper<Item const>> items) const {
  auto itemsFit = itemsFitWhere(items);
  std::uint64_t stackable = 0;
  for (auto slot : itemsFit.slots)
    if (m_items[slot])
      stackable += stackTransfer(!m_items[slot]->empty() ? optionalRef(m_items[slot]) : std::nullopt, items);
  return stackable;
}

auto ItemBag::itemsFitWhere(std::optional<std::reference_wrapper<Item const>> items, std::uint64_t max) const -> ItemsFitWhereResult {
  if (!items || items->get().empty())
    return ItemsFitWhereResult();

  List<std::size_t> slots;
  StableHashSet<std::size_t> taken;
  std::uint64_t count = std::min(items->get().count(), max);

  while (true) {
    if (count == 0)
      break;

    std::size_t slot = bestSlotAvailable(items->get(), false, [&](std::size_t i) {
      return !taken.contains(i);
    });
    if (slot == NPos)
      break;
    else {
      slots.push_back(slot);
      taken.insert(slot);
    }

    std::uint64_t available = stackTransfer(m_items[slot] && !m_items[slot]->empty() ? optionalRef(m_items[slot]) : std::nullopt, items);
    if (available != 0)
      count -= std::min(available, count);
    else
      break;
  }

  return ItemsFitWhereResult{count, slots};
}

std::expected<std::shared_ptr<Item>, StarException> ItemBag::addItems(std::shared_ptr<Item> items) {
  if (!items || items->empty())
    return std::shared_ptr<Item>();

  while (true) {
    std::size_t slot = bestSlotAvailable(*items, false);
    if (slot == NPos)
      return items;

    auto& storedItem = m_items[slot];
    if (storedItem && storedItem->empty())
      storedItem.reset();
    if (storedItem) {
      storedItem->stackWith(optionalRef(items));
      if (items->empty())
        return std::shared_ptr<Item>();
    } else {
      storedItem = std::move(items);
      return std::shared_ptr<Item>();
    }
  }
}

std::expected<std::shared_ptr<Item>, StarException> ItemBag::stackItems(std::shared_ptr<Item> items) {
  if (!items || items->empty())
    return std::shared_ptr<Item>();

  while (true) {
    std::size_t slot = bestSlotAvailable(*items, true);
    if (slot == NPos)
      return items;

    auto& storedItem = m_items[slot];
    if (storedItem && storedItem->empty())
      storedItem.reset();
    if (storedItem) {
      storedItem->stackWith(optionalRef(items));
      if (items->empty())
        return std::shared_ptr<Item>();
    } else {
      storedItem = std::move(items);
      return std::shared_ptr<Item>();
    }
  }
}

std::expected<void, StarException> ItemBag::condenseStacks() {
  cleanup();
  for (std::size_t i = size(); i > 1;) {
    --i;
    if (auto& item = m_items[i]) {
      for (std::size_t j = 0; j < i; ++j) {
        if (auto& stackWithItem = m_items[j])
          item->stackWith(optionalRef(stackWithItem));
        if (item->empty())
          break;
      }
    }
  }
  return {};
}

std::expected<void, StarException> ItemBag::read(DataStream& ds) {
  auto itemDatabase = m_root.get().itemDatabase();
  if (!itemDatabase)
    return std::unexpected(std::move(itemDatabase.error()));
  auto size = ds.readVlqU();
  if (!size)
    return std::unexpected(std::move(size.error()));
  m_items.clear();
  m_items.resize(*size);
  auto setItemsSize = ds.readVlqU();
  if (!setItemsSize)
    return std::unexpected(std::move(setItemsSize.error()));
  for (std::size_t i = 0; i < *setItemsSize; ++i) {
    auto descriptor = ds.read<ItemDescriptor>();
    if (!descriptor)
      return std::unexpected(std::move(descriptor.error()));
    auto slot = at(i);
    if (!slot)
      return std::unexpected(std::move(slot.error()));
    auto loaded = (*itemDatabase)->loadItem(*descriptor, slot->get());
    if (!loaded)
      return std::unexpected(std::move(loaded.error()));
  }
  return {};
}

std::expected<void, StarException> ItemBag::write(DataStream& ds) const {
  auto size = ds.writeVlqU(m_items.size());
  if (!size)
    return std::unexpected(std::move(size.error()));
  std::size_t setItemsSize = 0;
  for (std::size_t i = 0; i < m_items.size(); ++i) {
    if (m_items[i] && !m_items[i]->empty())
      setItemsSize = i + 1;
  }
  auto writtenSize = ds.writeVlqU(setItemsSize);
  if (!writtenSize)
    return std::unexpected(std::move(writtenSize.error()));
  for (std::size_t i = 0; i < setItemsSize; ++i) {
    auto const& item = m_items[i];
    auto written = ds.write(itemSafeDescriptor(item && !item->empty() ? item : std::shared_ptr<Item>()));
    if (!written)
      return std::unexpected(std::move(written.error()));
  }
  return {};
}

std::uint64_t ItemBag::stackTransfer(std::optional<std::reference_wrapper<Item const>> to, std::optional<std::reference_wrapper<Item const>> from) {
  if (!from)
    return 0;
  else if (!to)
    return from->get().count();
  else if (!to->get().stackableWith(from))
    return 0;
  else
    return std::min(to->get().maxStack() - to->get().count(), from->get().count());
}

std::size_t ItemBag::bestSlotAvailable(Item const& item, bool stacksOnly, std::function<bool(std::size_t)> test) const {
  // First look for any slots that can stack, before empty slots.
  for (std::size_t i = 0; i < m_items.size(); ++i) {
    if (!test(i))
      continue;
    auto const& storedItem = m_items[i];
    if (storedItem && !storedItem->empty() && stackTransfer(optionalRef(storedItem), std::cref(item)) != 0)
      return i;
  }

  if (!stacksOnly) {
    // Then, look for any empty slots.
    for (std::size_t i = 0; i < m_items.size(); ++i) {
      if (!m_items[i] || m_items[i]->empty())
        return i;
    }
  }

  return NPos;
}

std::size_t ItemBag::bestSlotAvailable(Item const& item, bool stacksOnly) const {
  return bestSlotAvailable(item, stacksOnly, [](std::size_t) { return true; });
}

}
