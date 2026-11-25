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

namespace {
enum class BagFormat { Descriptor, Disk };

std::expected<ItemBag, StarException> decode_bag(Root& root, Json const& store, BagFormat format) {
  auto database = root.itemDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto entries = store.array_ptr();
  if (!entries)
    return std::unexpected(std::move(entries.error()));
  ItemBag bag(root, (*entries)->size());
  auto& slots = bag.items();
  std::size_t index = 0;
  for (auto const& entry : **entries) {
    auto item = format == BagFormat::Disk ? (*database)->disk_load(entry) : (*database)->from_json(entry);
    if (!item)
      return std::unexpected(std::move(item.error()));
    slots[index++] = std::move(*item);
  }
  return bag;
}
}

std::expected<ItemBag, StarException> ItemBag::from_json(Root& root, Json const& store) {
  return decode_bag(root, store, BagFormat::Descriptor);
}

std::expected<ItemBag, StarException> ItemBag::load_store(Root& root, Json const& store) {
  return decode_bag(root, store, BagFormat::Disk);
}

std::expected<Json, StarException> ItemBag::to_json() const {
  JsonArray entries;
  entries.reserve(m_items.size());
  for (auto const& item : m_items)
    entries.push_back(item ? item->descriptor().to_json() : Json());
  return Json(std::move(entries));
}

std::expected<Json, StarException> ItemBag::disk_store() const {
  auto database = m_root.get().versioningDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  JsonArray entries;
  entries.reserve(m_items.size());
  for (auto const& item : m_items) {
    if (!item) {
      entries.push_back(Json());
      continue;
    }
    auto stored = item->descriptor().disk_store(**database);
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
    auto last_item = std::move(m_items[m_items.size() - 1]);
    m_items.resize(m_items.size() - 1);
    auto remaining = add_items(std::move(last_item));
    if (!remaining)
      return std::unexpected(std::move(remaining.error()));
    if (*remaining && !(*remaining)->empty())
      lost.push_back(std::move(*remaining));
  }
  m_items.resize(size);
  return lost;
}

void ItemBag::clear_items() {
  std::size_t old_size = m_items.size();
  m_items.clear();
  m_items.resize(old_size);
}

bool ItemBag::cleanup() const {
  bool cleanup_done = false;
  for (auto& item : m_items) {
    if (item && item->empty()) {
      cleanup_done = true;
      item.reset();
    }
  }

  return cleanup_done;
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

List<std::shared_ptr<Item>> ItemBag::take_all() {
  cleanup();
  List<std::shared_ptr<Item>> taken;
  for (auto& item : m_items) {
    if (item)
      taken.push_back(std::move(item));
  }
  return taken;
}

std::expected<void, StarException> ItemBag::set_item(std::size_t pos, std::shared_ptr<Item> item) {
  auto stored_item = at(pos);
  if (!stored_item)
    return std::unexpected(std::move(stored_item.error()));
  stored_item->get() = std::move(item);
  return {};
}

std::expected<std::shared_ptr<Item>, StarException> ItemBag::put_items(std::size_t pos, std::shared_ptr<Item> items) {
  if (!items || items->empty())
    return std::shared_ptr<Item>();
  auto slot = at(pos);
  if (!slot)
    return std::unexpected(std::move(slot.error()));
  auto& stored_item = slot->get();
  if (stored_item) {
    stored_item->stack_with(optional_ref(items));
    if (!items->empty())
      return items;
  } else {
    stored_item = std::move(items);
  }
  return std::shared_ptr<Item>();
}

std::expected<std::shared_ptr<Item>, StarException> ItemBag::take_items(std::size_t pos, std::uint64_t count) {
  auto slot = at(pos);
  if (!slot)
    return std::unexpected(std::move(slot.error()));
  auto& stored_item = slot->get();
  if (!stored_item)
    return std::shared_ptr<Item>();
  auto taken = stored_item->take(count);
  if (!taken)
    return std::unexpected(std::move(taken.error()));
  if (stored_item->empty())
    stored_item.reset();
  return taken;
}

std::expected<std::shared_ptr<Item>, StarException> ItemBag::swap_items(std::size_t pos, std::shared_ptr<Item> items, bool try_combine) {
  auto slot = at(pos);
  if (!slot)
    return std::unexpected(std::move(slot.error()));
  auto& stored_item = slot->get();

  // Empty input removes the stored stack; an empty slot adopts the input.
  if (!items || items->empty())
    return std::move(stored_item);
  if (!stored_item) {
    stored_item = std::move(items);
    return std::shared_ptr<Item>();
  }

  // Preserve the unmerged input (even when the merge exhausts it).
  if (!try_combine || !stored_item->stack_with(optional_ref(items)))
    std::swap(stored_item, items);
  return items;
}

std::expected<bool, StarException> ItemBag::consume_items(std::size_t pos, std::uint64_t count) {
  auto slot = at(pos);
  if (!slot)
    return std::unexpected(std::move(slot.error()));
  auto& stored_item = slot->get();
  bool consumed = false;
  if (stored_item) {
    consumed = stored_item->consume(count);
    if (stored_item->empty())
      stored_item.reset();
  }
  return consumed;
}

bool ItemBag::consume_items(ItemDescriptor const& descriptor, bool exact_match) {
  std::uint64_t count_left = descriptor.count();
  List<std::pair<std::size_t, std::uint64_t>> consume_locations;
  for (std::size_t i = 0; i < m_items.size(); ++i) {
    auto& stored_item = m_items[i];
    if (stored_item && stored_item->empty())
      stored_item.reset();
    if (stored_item && stored_item->matches(descriptor, exact_match)) {
      std::uint64_t count = stored_item->count();
      std::uint64_t take = std::min(count, count_left);
      consume_locations.push_back({i, take});
      count_left -= take;
      if (count_left == 0)
        break;
    }
  }

  // Only consume any if we can consume them all
  if (count_left > 0)
    return false;

  for (auto const& [index, count] : consume_locations) {
    auto& stored_item = m_items[index];
    stored_item->consume(count);
    if (stored_item->empty())
      stored_item.reset();
  }

  return true;
}

std::uint64_t ItemBag::matching_count(ItemDescriptor const& descriptor, bool exact_match) const {
  std::uint64_t count = 0;
  for (auto const& items : m_items) {
    if (items && items->matches(descriptor, exact_match))
      count += std::min(items->count(), std::numeric_limits<std::uint64_t>::max() - count);
  }

  return count;
}

std::uint64_t ItemBag::available(ItemDescriptor const& descriptor, bool exact_match) const {
  auto batch = descriptor.count();
  return batch ? matching_count(descriptor, exact_match) / batch : 0;
}

namespace {

constexpr auto ignore_slot = [](std::size_t) {};

template <typename RecordSlot>
  requires std::invocable<RecordSlot&, std::size_t&>
std::uint64_t unplaced_items(List<std::shared_ptr<Item>> const& slots,
    std::optional<std::reference_wrapper<Item const>> source, std::uint64_t remaining,
    bool include_vacant, RecordSlot record_slot) {
  std::size_t first_vacant = npos;
  for (std::size_t i = 0; remaining && i < slots.size(); ++i) {
    auto const& target = slots[i];
    if (!target || target->empty()) {
      if (first_vacant == npos)
        first_vacant = i;
    } else if (auto capacity = target->could_stack(source)) {
      record_slot(i);
      remaining -= std::min(remaining, capacity);
    }
  }

  // Existing stacks take precedence. The legacy stack-only query counts a
  // retained zero stack if it was the first vacant slot, but not a null slot.
  if (remaining && first_vacant != npos && (include_vacant || slots[first_vacant])) {
    record_slot(first_vacant);
    return 0;
  }
  return remaining;
}

std::shared_ptr<Item> transfer_items(List<std::shared_ptr<Item>>& slots,
    std::shared_ptr<Item> source, bool include_vacant) {
  if (!source || source->empty())
    return {};

  std::size_t first_vacant = npos;
  for (std::size_t i = 0; i < slots.size(); ++i) {
    auto& target = slots[i];
    if (!target || target->empty()) {
      if (include_vacant && first_vacant == npos)
        first_vacant = i;
    } else if (target != source && target->stack_with(optional_ref(source)) && source->empty()) {
      return {};
    }
  }
  if (first_vacant != npos) {
    slots[first_vacant] = std::move(source);
    return {};
  }
  return source;
}

}

std::uint64_t ItemBag::items_can_fit(std::optional<std::reference_wrapper<Item const>> items) const {
  auto count = items ? items->get().count() : 0;
  return count - unplaced_items(m_items, items, count, true, ignore_slot);
}

std::uint64_t ItemBag::items_can_stack(std::optional<std::reference_wrapper<Item const>> items) const {
  auto count = items ? items->get().count() : 0;
  return count - unplaced_items(m_items, items, count, false, ignore_slot);
}

auto ItemBag::items_fit_where(std::optional<std::reference_wrapper<Item const>> items, std::uint64_t max) const
    -> ItemsFitWhereResult {
  auto count = items ? std::min(items->get().count(), max) : 0;
  ItemsFitWhereResult fit{};
  fit.leftover = unplaced_items(m_items, items, count, true, [&](std::size_t slot) { fit.slots.push_back(slot); });
  return fit;
}

std::expected<std::shared_ptr<Item>, StarException> ItemBag::add_items(std::shared_ptr<Item> items) {
  return transfer_items(m_items, std::move(items), true);
}

std::expected<std::shared_ptr<Item>, StarException> ItemBag::stack_items(std::shared_ptr<Item> items) {
  return transfer_items(m_items, std::move(items), false);
}

std::expected<void, StarException> ItemBag::condense_stacks() {
  std::size_t first_donor = 0;
  for (std::size_t i = m_items.size(); i > 0;) {
    auto& target = m_items[--i];
    if (!target)
      continue;
    if (target->empty()) {
      target.reset();
      continue;
    }
    if (target->count() >= target->max_stack())
      continue;

    // Higher slots receive first; each receives from lower slots in ascending order.
    for (std::size_t j = first_donor; j < i; ++j) {
      auto& donor = m_items[j];
      if (donor) {
        if (donor != target)
          target->stack_with(optional_ref(donor));
        if (donor->empty())
          donor.reset();
      }
      if (j == first_donor && !donor)
        ++first_donor;
      if (target->count() >= target->max_stack())
        break;
    }
  }
  return {};
}

std::expected<void, StarException> ItemBag::read(DataStream& ds) {
  auto database = m_root.get().itemDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto size = ds.read_vlq_u();
  if (!size)
    return std::unexpected(std::move(size.error()));
  auto prefix = ds.read_vlq_u();
  if (!prefix)
    return std::unexpected(std::move(prefix.error()));
  List<std::shared_ptr<Item>> slots;
  if (*prefix > *size || *size > slots.max_size())
    return std::unexpected(ItemException("Invalid item bag dimensions"));
  slots.resize(*size);
  for (std::size_t i = 0; i < *prefix; ++i) {
    auto descriptor = ds.read<ItemDescriptor>();
    if (!descriptor)
      return std::unexpected(std::move(descriptor.error()));
    auto loaded = (*database)->load_item(*descriptor, slots[i]);
    if (!loaded)
      return std::unexpected(std::move(loaded.error()));
  }
  m_items = std::move(slots);
  return {};
}

std::expected<void, StarException> ItemBag::write(DataStream& ds) const {
  auto size = ds.write_vlq_u(m_items.size());
  if (!size)
    return std::unexpected(std::move(size.error()));
  auto prefix = m_items.size();
  while (prefix && (!m_items[prefix - 1] || m_items[prefix - 1]->empty()))
    --prefix;
  auto written_size = ds.write_vlq_u(prefix);
  if (!written_size)
    return std::unexpected(std::move(written_size.error()));
  for (std::size_t i = 0; i < prefix; ++i) {
    auto const& item = m_items[i];
    auto written = ds.write(item && !item->empty() ? item->descriptor() : ItemDescriptor());
    if (!written)
      return std::unexpected(std::move(written.error()));
  }
  return {};
}

}
