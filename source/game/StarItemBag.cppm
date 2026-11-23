module;
import std;
import star.format;
import star.list;
import star.json;

import star.math_common;
import star.data_stream;
import star.bi_map;
import star.item_descriptor;

namespace Star {

class Item;
class Root;

// Manages a collection of items with non-zero counts, and putting them in /
// stacking them / consuming them.  As items are taken out of the ItemBag, any
// Item with a zero count is set to null, so no shared item pointer returned by this
// class should ever be empty.  They will either be null, or of count >= 1.
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
  static std::expected<ItemBag, StarException> fromJson(Root& root, Json const& spec);
  [[nodiscard]]
  static std::expected<ItemBag, StarException> loadStore(Root& root, Json const& store);

  [[nodiscard]]
  std::expected<Json, StarException> toJson() const;
  [[nodiscard]]
  std::expected<Json, StarException> diskStore() const;

  std::size_t size() const;
  // May reshape the container, but will try not to lose any container
  // contents.  Returns overflow.
  [[nodiscard]]
  std::expected<List<std::shared_ptr<Item>>, StarException> resize(std::size_t size);
  // Clears all item slots, does not change ItemBag size
  void clearItems();

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
  List<std::shared_ptr<Item>> takeAll();

  // Directly set the value of an item at a given slot
  [[nodiscard]]
  std::expected<void, StarException> setItem(std::size_t pos, std::shared_ptr<Item> item);

  // Put items into the given slot.  Returns number of items left over
  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> putItems(std::size_t pos, std::shared_ptr<Item> items);
  // Take a maximum number of items from the given position, defaults to all.
  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> takeItems(std::size_t pos, std::uint64_t count = std::numeric_limits<std::uint64_t>::max());
  // Put items in the slot by combining, or swap the current items with the
  // given items.
  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> swapItems(std::size_t pos, std::shared_ptr<Item> items, bool tryCombine = true);

  // Destroys the given number of items, only if the entirety of count is
  // available, returns success.
  [[nodiscard]]
  std::expected<bool, StarException> consumeItems(std::size_t pos, std::uint64_t count);

  // Consume any items from any stack that matches the given item descriptor,
  // only if the entirety of the count is available.  Returns success.
  bool consumeItems(ItemDescriptor const& descriptor, bool exactMatch = false);

  // Returns the number of times this ItemDescriptor could be consumed using
  // the items in this container.
  std::uint64_t available(ItemDescriptor const& descriptor, bool exactMatch = false) const;

  // Returns the number of items that can fit anywhere in the bag, including
  // being split up.
  std::uint64_t itemsCanFit(std::optional<std::reference_wrapper<Item const>> items) const;
  // Returns the number of items that can be stacked with existing items
  // anywhere in the bag.
  std::uint64_t itemsCanStack(std::optional<std::reference_wrapper<Item const>> items) const;

  // Returns where the items would fit if inserted, including any splitting up
  ItemsFitWhereResult itemsFitWhere(std::optional<std::reference_wrapper<Item const>> items, std::uint64_t max = std::numeric_limits<std::uint64_t>::max()) const;

  // Add items anywhere in the bag. Tries to stack items first.  If any items
  // are left over, addItems returns them, otherwise null.
  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> addItems(std::shared_ptr<Item> items);

  // Add items to the bag, but only if they stack with existing items in the
  // bag.
  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> stackItems(std::shared_ptr<Item> items);

  // Attempt to condense all stacks in the given bag
  [[nodiscard]]
  std::expected<void, StarException> condenseStacks();

  // Resolves the current ItemDatabase through the borrowed Root when reading.
  [[nodiscard]]
  std::expected<void, StarException> read(DataStream& ds);
  [[nodiscard]]
  std::expected<void, StarException> write(DataStream& ds) const;

private:
  // If the from item can stack into the given to item, returns the amount that
  // would be transfered.
  static std::uint64_t stackTransfer(std::optional<std::reference_wrapper<Item const>> to, std::optional<std::reference_wrapper<Item const>> from);

  // Returns the slot that contains the item already and has the *highest*
  // stack count but not full, or an empty slot, or npos for no room.
  std::size_t bestSlotAvailable(Item const& item, bool stacksOnly, std::function<bool(std::size_t)> test) const;
  std::size_t bestSlotAvailable(Item const& item, bool stacksOnly) const;

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
