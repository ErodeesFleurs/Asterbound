module;
import std;
import star.string;
import star.list;
import star.map;
import star.format;
import star.json;

import star.bi_map;
import star.strong_typedef;
import star.inventory_types;
import star.multi_array;
import star.data_stream;
import star.item_descriptor;

import star.net_element_basic_fields;
import star.net_element_sync_group;

import star.either;
import star.net_compatibility;
namespace Star {

class Item;
class ItemBag;
class ArmorItem;
class HeadArmor;
class ChestArmor;
class LegsArmor;
class BackArmor;
class Player;
class Root;

struct InventoryExceptionTag {
  static constexpr std::string_view name() { return "InventoryException"; }
};
using InventoryException = StarError<InventoryExceptionTag, StarException>;

// Describes a player's entire inventory, including the main bag, material bag,
// object bag, reagent bag, food bag, weapon and armor slots, swap slot, trash
// slot, essential items, and currencies.
//
// Items in the inventory can be shorcutted in the "Action Bar", and one
// location in the action bar is selected at a time and the primary and
// secondary held items are the items pointed to in that action bar location.
//
// The special slot called the "swap" slot is used specifically for inventory
// management and is attached to the cursor.  When the swap slot is active,
// then whatever is in the slot swap temporarily becomes the only held item.
//
// The essential items are items that are not manageable and not pointable to
// by an ItemSlot, but are part of the action bar shortcut system.  They are
// used for permanent tools that need to be always quickly available.
//
// Currency items that enter the inventory are immediately put in the common currencies
// pool, and are also not manageable items.
class PlayerInventory : public NetElementSyncGroup {
public:
  // Whether the given item is allowed to go in the given slot type
  [[nodiscard]] std::expected<bool, StarException> itemAllowedInBag(std::shared_ptr<Item> const& item, String const& bagType) const;
  static bool itemAllowedAsEquipment(std::shared_ptr<Item> const& item, EquipmentSlot equipmentSlot);

  [[nodiscard]] static std::expected<std::shared_ptr<PlayerInventory>, StarException> create(Root& root);

  // Items may be consumed through shared aliases. Queries observe exhausted
  // stacks as empty without releasing the inventory's ownership.
  [[nodiscard]] std::expected<std::shared_ptr<Item>, StarException> itemsAt(InventorySlot const& slot) const;

  // Attempts to combine the items with the given slot, and returns the items
  // left over (if any).
  [[nodiscard]] std::expected<std::shared_ptr<Item>, StarException> stackWith(InventorySlot const& slot, std::shared_ptr<Item> const& items);

  // Empty the slot and take what it contains, if any.
  [[nodiscard]] std::expected<std::shared_ptr<Item>, StarException> takeSlot(InventorySlot const& slot);

  // Try to exchange items between any two slots, returns true on success.
  [[nodiscard]] std::expected<bool, StarException> exchangeItems(InventorySlot const& first, InventorySlot const& second);

  // Forces the given item into the given slot, overriding what was already
  // there.  If the item is not allowed in the given location, does nothing and
  // returns false.
  [[nodiscard]] std::expected<bool, StarException> setItem(InventorySlot const& slot, std::shared_ptr<Item> const& item);

  [[nodiscard]] std::expected<bool, StarException> consumeSlot(InventorySlot const& slot, std::uint64_t count = 1);

  bool slotValid(InventorySlot const& slot) const;

  // Adds items to any slot except the trash or swap slots, returns stack left
  // over.
  [[nodiscard]] std::expected<std::shared_ptr<Item>, StarException> addItems(std::shared_ptr<Item> items);

  // Adds items to the first matching item bag, avoiding the equipment, swap,
  // or trash slots
  [[nodiscard]] std::expected<std::shared_ptr<Item>, StarException> addToBags(std::shared_ptr<Item> items);

  // Returns number of items in the given set that can fit anywhere in any item
  // slot except the trash slot (the number of items that would be added by a
  // call to addItems).
  [[nodiscard]] std::expected<std::uint64_t, StarException> itemsCanFit(std::shared_ptr<Item> const& items) const;

  bool hasItem(ItemDescriptor const& descriptor, bool exactMatch = false) const;
  std::uint64_t hasCountOfItem(ItemDescriptor const& descriptor, bool exactMatch = false) const;

  // Consume items based on ItemDescriptor. Can take from any manageable item slot.
  bool consumeItems(ItemDescriptor const& descriptor, bool exactMatch = false);
  [[nodiscard]] std::expected<ItemDescriptor, StarException> takeItems(ItemDescriptor const& descriptor, bool takePartial = false, bool exactMatch = false);
  // Return a summary of every item that can be consumed by ItemDescriptor.
  [[nodiscard]] std::expected<HashMap<ItemDescriptor, std::uint64_t>, StarException> availableItems() const;

  std::shared_ptr<HeadArmor> headArmor() const;
  std::shared_ptr<ChestArmor> chestArmor() const;
  std::shared_ptr<LegsArmor> legsArmor() const;
  std::shared_ptr<BackArmor> backArmor() const;

  std::shared_ptr<HeadArmor> headCosmetic() const;
  std::shared_ptr<ChestArmor> chestCosmetic() const;
  std::shared_ptr<LegsArmor> legsCosmetic() const;
  std::shared_ptr<BackArmor> backCosmetic() const;

  std::shared_ptr<ArmorItem> equipment(EquipmentSlot slot, bool testMask = false) const;

  std::shared_ptr<ItemBag const> bagContents(String const& bag) const;

  [[nodiscard]] std::expected<void, StarException> condenseBagStacks(String const& bag);

  // Sorting a bag will not change the contents of an action bar location.  It
  // will instead potentially change the pointed to slot of an action bar
  // location to point to the new slot that contains the same item.
  [[nodiscard]] std::expected<void, StarException> sortBag(String const& bag);

  // Either move the contents of the given slot into the swap slot, move the
  // contents of the swap slot into the given inventory slot, or swap the
  // contents of the swap slot and the inventory slot, or combine them,
  // whichever makes the most sense.
  [[nodiscard]] std::expected<void, StarException> shiftSwap(InventorySlot const& slot);

  // Puts the swap slot back into the inventory, if there is room.  Returns
  // true if this was successful, and the swap slot is now empty.
  [[nodiscard]] std::expected<bool, StarException> clearSwap();

  std::shared_ptr<Item> swapSlotItem() const;
  [[nodiscard]] std::expected<void, StarException> setSwapSlotItem(std::shared_ptr<Item> const& items);

  // Non-manageable essential items that are always available as action bar
  // entries.
  std::shared_ptr<Item> essentialItem(EssentialItem essentialItem) const;
  void setEssentialItem(EssentialItem essentialItem, std::shared_ptr<Item> item);

  // Non-manageable currencies
  StringMap<std::uint64_t> availableCurrencies() const;
  std::uint64_t currency(String const& currencyType) const;
  [[nodiscard]] std::expected<void, StarException> addCurrency(String const& currencyType, std::uint64_t amount);
  bool consumeCurrency(String const& currencyType, std::uint64_t amount);

  // A custom bar location primary and secondary cannot point to a slot that
  // has no item, and rather than set an empty slot to that location, the slot
  // will simply be cleared.  If a primary slot is set to a two handed item, it
  // will clear the secondary slot.  Any secondary slot that is set must be a
  // one handed item.
  [[nodiscard]] std::expected<std::optional<InventorySlot>, StarException> customBarPrimarySlot(CustomBarIndex customBarIndex) const;
  [[nodiscard]] std::expected<std::optional<InventorySlot>, StarException> customBarSecondarySlot(CustomBarIndex customBarIndex) const;
  [[nodiscard]] std::expected<void, StarException> setCustomBarPrimarySlot(CustomBarIndex customBarIndex, std::optional<InventorySlot> slot);
  [[nodiscard]] std::expected<void, StarException> setCustomBarSecondarySlot(CustomBarIndex customBarIndex, std::optional<InventorySlot> slot);

  // Link the cursor's real item and return it to inventory as one operation.
  // A two-handed cursor item always occupies the primary link.
  [[nodiscard]] std::expected<void, StarException> linkCustomBarSwap(CustomBarIndex index, bool primary);
  // Clearing the displayed secondary half of a two-handed item clears primary.
  [[nodiscard]] std::expected<void, StarException> clearCustomBarLink(CustomBarIndex index, bool primary);

  // Add the given slot to a free place in the custom bar if one is available.
  [[nodiscard]] std::expected<void, StarException> addToCustomBar(InventorySlot slot);

  // The custom bar has 'CustomBarGroups' groups that can be switched between.
  // This will not change the selected action bar location, but may change the
  // item if the selected location points to the custom bar and the contents
  // change.
  std::uint8_t customBarGroup() const;
  void setCustomBarGroup(std::uint8_t group);
  std::uint8_t customBarGroups() const;
  std::uint8_t customBarIndexes() const;

  // The action bar is the combination of the custom bar and the essential
  // items, and any of these locations can be selected.
  SelectedActionBarLocation selectedActionBarLocation() const;
  void selectActionBarLocation(SelectedActionBarLocation selectedActionBarLocation);

  // Held items are either the items shortcutted to in the currently selected
  // ActionBar primary / secondary locations, or if the swap slot is non-empty
  // then the swap slot.
  [[nodiscard]] std::expected<std::shared_ptr<Item>, StarException> primaryHeldItem() const;
  [[nodiscard]] std::expected<std::shared_ptr<Item>, StarException> secondaryHeldItem() const;

  // If the primary / secondary held items are valid manageable slots, returns
  // them.
  [[nodiscard]] std::expected<std::optional<InventorySlot>, StarException> primaryHeldSlot() const;
  [[nodiscard]] std::expected<std::optional<InventorySlot>, StarException> secondaryHeldSlot() const;

  List<std::shared_ptr<Item>> pullOverflow();
  void setEquipmentVisibility(EquipmentSlot slot, bool visible);
  bool equipmentVisibility(EquipmentSlot slot) const;

  [[nodiscard]] std::expected<void, StarException> load(Json const& store);
  [[nodiscard]] std::expected<Json, StarException> store() const;

  // Checked traversal retains normalization after each successful mutation.
  [[nodiscard]] std::expected<void, StarException> forEveryItem(std::function<std::expected<void, StarException>(InventorySlot const&, std::shared_ptr<Item>&)> function);
  [[nodiscard]] std::expected<void, StarException> forEveryItem(std::function<std::expected<void, StarException>(InventorySlot const&, std::shared_ptr<Item> const&)> function) const;
  // Return every manageable item
  [[nodiscard]] std::expected<List<std::shared_ptr<Item>>, StarException> allItems() const;
  // Return summary of every manageable item name and the count of that item
  [[nodiscard]] std::expected<Map<String, std::uint64_t>, StarException> itemSummary() const;

  // Clears away any empty items and sets them as null, and updates action bar
  // slots to maintain the rules for the action bar.  Should be called every
  // tick.
  [[nodiscard]] std::expected<void, StarException> cleanup();

  // Nullable while unbound; the player clears this borrow before destruction.
  void setPlayer(std::optional<std::reference_wrapper<Player>> player);

  // Empty network state with this inventory's original bag and action-bar schema.
  [[nodiscard]] std::expected<std::reference_wrapper<PlayerInventory const>, StarException> blankInventory() const;

  [[nodiscard]] std::expected<void, StarException> netStore(DataStream& ds, NetCompatibilityRules rules = {}) const override;
  [[nodiscard]] std::expected<bool, StarException> writeNetDelta(DataStream& ds, std::uint64_t fromVersion, NetCompatibilityRules rules = {}) const override;

private:
  using CustomBarLink = std::pair<std::optional<InventorySlot>, std::optional<InventorySlot>>;

  template <typename> friend class NetElementTop;
  explicit PlayerInventory(Root& root);
  PlayerInventory(Root& root, Json inventoryConfig, Json currenciesConfig);
  [[nodiscard]] static std::expected<std::shared_ptr<PlayerInventory>, StarException> create(Root& root, Json inventoryConfig, Json currenciesConfig);
  [[nodiscard]] std::expected<void, StarException> initialize();

  [[nodiscard]] std::expected<bool, StarException> checkInventoryFilter(std::shared_ptr<Item> const& items, String const& filterName) const;

  static void normalizeItem(std::shared_ptr<Item>& item);
  [[nodiscard]] std::expected<std::shared_ptr<Item>, StarException> retrieve(InventorySlot const& slot) const;
  [[nodiscard]] std::expected<std::reference_wrapper<std::shared_ptr<Item>>, StarException> retrieveForMutation(InventorySlot const& slot);

  void swapCustomBarLinks(InventorySlot a, InventorySlot b);
  [[nodiscard]] std::expected<void, StarException> autoAddToCustomBar(InventorySlot slot);

  [[nodiscard]] std::expected<void, StarException> netElementsNeedLoad(bool full) override;
  [[nodiscard]] std::expected<void, StarException> netElementsNeedStore() override;

  // Borrowed for this inventory's lifetime; resource lookups remain reload-aware.
  Root& m_root;
  // Preserve the construction schema for the per-inventory empty network state.
  Json m_inventoryConfig;
  Json m_currenciesConfig;
  mutable std::shared_ptr<PlayerInventory> m_blankInventory;

  Map<EquipmentSlot, std::shared_ptr<Item>> m_equipment;
  Map<String, std::shared_ptr<ItemBag>> m_bags;
  std::shared_ptr<Item> m_swapSlot;
  std::optional<InventorySlot> m_swapReturnSlot;
  std::shared_ptr<Item> m_trashSlot;
  Map<EssentialItem, std::shared_ptr<Item>> m_essential;
  StringMap<std::uint64_t> m_currencies;
  std::uint8_t m_customBarGroup;
  MultiArray<CustomBarLink, 2> m_customBar;
  SelectedActionBarLocation m_selectedActionBar;

  Map<EquipmentSlot, NetElementData<ItemDescriptor>> m_equipmentNetState;
  Map<String, List<NetElementData<ItemDescriptor>>> m_bagsNetState;
  NetElementData<ItemDescriptor> m_swapSlotNetState;
  NetElementData<ItemDescriptor> m_trashSlotNetState;
  Map<EssentialItem, NetElementData<ItemDescriptor>> m_essentialNetState;
  NetElementData<StringMap<std::uint64_t>> m_currenciesNetState;
  NetElementUInt m_customBarGroupNetState;
  MultiArray<NetElementData<CustomBarLink>, 2> m_customBarNetState;
  NetElementData<SelectedActionBarLocation> m_selectedActionBarNetState;

  List<std::shared_ptr<Item>> m_inventoryLoadOverflow;
  unsigned m_equipmentVisibilityMask;

  std::optional<std::reference_wrapper<Player>> m_player;
};

}

export module star.player_inventory;

export namespace Star {
using ::Star::Item;
using ::Star::ItemBag;
using ::Star::ArmorItem;
using ::Star::HeadArmor;
using ::Star::ChestArmor;
using ::Star::LegsArmor;
using ::Star::BackArmor;
using ::Star::Player;
using ::Star::PlayerInventory;
using ::Star::InventoryExceptionTag;
using ::Star::InventoryException;
}
