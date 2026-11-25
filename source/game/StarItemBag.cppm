module;
import std;
import star.format;
import star.list;
import star.json;

import star.data_stream;
import star.item_descriptor;

namespace Star {

class Item;
class Root;

// Stores and transfers item stacks. A stack with zero count is logically empty,
// though its shared pointer can remain in a slot until that slot is normalized.
// Nullable item parameters accept null. Query parameters borrow their items only
// for the call. Owning item parameters may be retained internally depending on
// how the item stacks, so should not be reused after passing to the method.
class ItemBag {
public:
  struct ItemsFitWhereResult {
    std::uint64_t leftover;
    List<std::size_t> slots;
  };

  explicit ItemBag(Root& root, std::size_t size = 0);

  [[nodiscard]]
  static std::expected<ItemBag, StarException> from_json(Root& root, Json const& spec);
  [[nodiscard]]
  static std::expected<ItemBag, StarException> load_store(Root& root, Json const& store);

  [[nodiscard]]
  std::expected<Json, StarException> to_json() const;
  [[nodiscard]]
  std::expected<Json, StarException> disk_store() const;

  std::size_t size() const;
  // May reshape the container, but will try not to lose any container
  // contents.  Returns overflow.
  [[nodiscard]]
  std::expected<List<std::shared_ptr<Item>>, StarException> resize(std::size_t size);
  // Clears all item slots, does not change ItemBag size
  void clear_items();

  // Normalize empty item stacks to null slots. Direct item-list access also
  // performs this normalization, including through the const overload.
  // Returns true if any empty stack was released.
  bool cleanup() const;

  // Direct access to item list
  List<std::shared_ptr<Item>>& items();
  List<std::shared_ptr<Item>> const& items() const;

  // Pure query: an externally exhausted stack is observed as an empty slot.
  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> at(std::size_t i) const;
  [[nodiscard]]
  std::expected<std::reference_wrapper<std::shared_ptr<Item>>, StarException> at(std::size_t i);

  // Returns all non-empty items and clears container contents
  List<std::shared_ptr<Item>> take_all();

  // Directly set the value of an item at a given slot
  [[nodiscard]]
  std::expected<void, StarException> set_item(std::size_t pos, std::shared_ptr<Item> item);

  // Put items into the given slot and return the leftover stack, if any.
  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> put_items(std::size_t pos, std::shared_ptr<Item> items);
  // Take a maximum number of items from the given position, defaults to all.
  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> take_items(std::size_t pos, std::uint64_t count = std::numeric_limits<std::uint64_t>::max());
  // Put items in the slot by combining, or swap the current items with the
  // given items.
  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> swap_items(std::size_t pos, std::shared_ptr<Item> items, bool try_combine = true);

  // Destroys the given number of items, only if the entirety of count is
  // available, returns success.
  [[nodiscard]]
  std::expected<bool, StarException> consume_items(std::size_t pos, std::uint64_t count);

  // Consume any items from any stack that matches the given item descriptor,
  // only if the entirety of the count is available.  Returns success.
  bool consume_items(ItemDescriptor const& descriptor, bool exact_match = false);

  // Physical units, independent of the requested stack size.
  std::uint64_t matching_count(ItemDescriptor const& descriptor, bool exact_match) const;

  // Returns the number of times this ItemDescriptor could be consumed using
  // the items in this container. A zero-sized request has no consumable batches.
  std::uint64_t available(ItemDescriptor const& descriptor, bool exact_match = false) const;

  // Returns the number of items that can fit anywhere in the bag, including
  // being split up.
  std::uint64_t items_can_fit(std::optional<std::reference_wrapper<Item const>> items) const;
  // Returns the number of items that can be stacked with existing items
  // anywhere in the bag.
  std::uint64_t items_can_stack(std::optional<std::reference_wrapper<Item const>> items) const;

  // Returns where the items would fit if inserted, including any splitting up
  ItemsFitWhereResult items_fit_where(std::optional<std::reference_wrapper<Item const>> items, std::uint64_t max = std::numeric_limits<std::uint64_t>::max()) const;

  // Add items anywhere in the bag. Tries to stack items first.  If any items
  // are left over, add_items returns them, otherwise null.
  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> add_items(std::shared_ptr<Item> items);

  // Add items to the bag, but only if they stack with existing items in the
  // bag.
  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> stack_items(std::shared_ptr<Item> items);

  // Attempt to condense all stacks in the given bag
  [[nodiscard]]
  std::expected<void, StarException> condense_stacks();

  // Resolves the current ItemDatabase through the borrowed Root when reading.
  [[nodiscard]]
  std::expected<void, StarException> read(DataStream& ds);
  [[nodiscard]]
  std::expected<void, StarException> write(DataStream& ds) const;

private:
  // Borrowed for the bag's lifetime; deserialization resolves the current catalog.
  std::reference_wrapper<Root> m_root;

  // Empty stacks and null slots represent the same logical contents.
  mutable List<std::shared_ptr<Item>> m_items;
};

}

export module star.item_bag;

export namespace Star {
using ::Star::Item;
using ::Star::ItemBag;
}
