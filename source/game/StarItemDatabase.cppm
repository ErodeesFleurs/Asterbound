module;
import std;
import star.map;
import star.format;
import star.json;
import star.thread;
import star.ordered_map;
import star.lru_cache;
import star.time;
import star.random;
import star.ttl_cache;
import star.data_stream;
import star.bi_map;
import star.game_types;
import star.set;
import star.string;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.strong_typedef;
import star.casting;


import star.item_descriptor;
import star.item_recipe;
import star.drawable;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.item;

import star.either;
namespace Star {

class Root;
class RecipeDatabase;
class AugmentItem;

class Rebuilder;
class LuaRoot;

struct ItemDatabaseExceptionTag {
  static constexpr std::string_view name() { return "ItemDatabaseException"; }
};
using ItemDatabaseException = StarError<ItemDatabaseExceptionTag, ItemException>;

enum class ItemType {
  Generic,
  LiquidItem,
  MaterialItem,
  ObjectItem,
  CurrencyItem,
  MiningTool,
  Flashlight,
  WireTool,
  BeamMiningTool,
  HarvestingTool,
  TillingTool,
  PaintingBeamTool,
  HeadArmor,
  ChestArmor,
  LegsArmor,
  BackArmor,
  Consumable,
  Blueprint,
  Codex,
  InspectionTool,
  InstrumentItem,
  GrapplingHook,
  ThrownItem,
  UnlockItem,
  ActiveItem,
  AugmentItem
};
inline constexpr auto ItemTypeNames = sortedEnumNames(std::to_array<std::pair<ItemType, std::string_view>>({
  {ItemType::Generic, "generic"},
  {ItemType::LiquidItem, "liquid"},
  {ItemType::MaterialItem, "material"},
  {ItemType::ObjectItem, "object"},
  {ItemType::CurrencyItem, "currency"},
  {ItemType::MiningTool, "miningtool"},
  {ItemType::Flashlight, "flashlight"},
  {ItemType::WireTool, "wiretool"},
  {ItemType::BeamMiningTool, "beamminingtool"},
  {ItemType::HarvestingTool, "harvestingtool"},
  {ItemType::TillingTool, "tillingtool"},
  {ItemType::PaintingBeamTool, "paintingbeamtool"},
  {ItemType::HeadArmor, "headarmor"},
  {ItemType::ChestArmor, "chestarmor"},
  {ItemType::LegsArmor, "legsarmor"},
  {ItemType::BackArmor, "backarmor"},
  {ItemType::Consumable, "consumable"},
  {ItemType::Blueprint, "blueprint"},
  {ItemType::Codex, "codex"},
  {ItemType::InspectionTool, "inspectiontool"},
  {ItemType::InstrumentItem, "instrument"},
  {ItemType::ThrownItem, "thrownitem"},
  {ItemType::UnlockItem, "unlockitem"},
  {ItemType::ActiveItem, "activeitem"},
  {ItemType::AugmentItem, "augmentitem"},
}));

class ItemDatabase {
public:
  // During item loading, the ItemDatabase takes the ItemDescriptor and
  // produces a set of things from it:
  struct ItemConfig {
    // The relative path in assets to the base config
    String directory;

    // A possibly modified / generated config from the base config that is
    // re-constructed each time an ItemDescriptor is loaded.  Become's the
    // Item's base config.
    Json config;

    // The parameters from the ItemDescriptor, also possibly modified during
    // loading.  Since this become's the Item's parameters, it will be
    // subsequently stored with the Item as the new ItemDescriptor.
    Json parameters;
  };

  static std::uint64_t getCountOfItem(List<std::shared_ptr<Item>> const& bag, ItemDescriptor const& item, bool exactMatch = false);
  static std::uint64_t getCountOfItem(HashMap<ItemDescriptor, std::uint64_t> const& bag, ItemDescriptor const& item, bool exactMatch = false);
  static HashMap<ItemDescriptor, std::uint64_t> normalizeBag(List<std::shared_ptr<Item>> const& bag);
  static bool canMakeRecipe(ItemRecipe const& recipe, HashMap<ItemDescriptor, std::uint64_t> const& availableIngredients, StringMap<std::uint64_t> const& availableCurrencies);
  static HashSet<ItemRecipe> recipesFromSubset(HashMap<ItemDescriptor, std::uint64_t> const& normalizedBag, StringMap<std::uint64_t> const& availableCurrencies, HashSet<ItemRecipe> const& subset);
  static HashSet<ItemRecipe> recipesFromSubset(HashMap<ItemDescriptor, std::uint64_t> const& normalizedBag, StringMap<std::uint64_t> const& availableCurrencies, HashSet<ItemRecipe> const& subset, StringSet const& allowedTypes);
  [[nodiscard]]
  static std::expected<String, StarException> guiFilterString(std::shared_ptr<Item> const& item);

  [[nodiscard]]
  static std::expected<std::shared_ptr<ItemDatabase>, StarException> create(Root& root);

  void cleanup();

  // Load an item based on item descriptor.  If loadItem is called with a
  // live ptr, and the ptr matches the descriptor read, then no new item is
  // constructed.  If ItemT is some other type than Item, then loadItem will
  // clear the item if the new item is not castable to it. Returns whether the
  // pointer changed. Existing recovery handlers may substitute a default item;
  // failures in those handlers or the fallback are propagated.
  template <typename ItemT>
  [[nodiscard]]
  std::expected<bool, StarException> loadItem(ItemDescriptor const& descriptor, std::shared_ptr<ItemT>& itemPtr) const;

  // Protects against re-instantiating an item in the same was as loadItem
  template <typename ItemT>
  [[nodiscard]]
  std::expected<bool, StarException> diskLoad(Json const& diskStore, std::shared_ptr<ItemT>& itemPtr) const;

  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> diskLoad(Json const& diskStore) const;
  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> fromJson(Json const& spec) const;

  [[nodiscard]]
  std::expected<Json, StarException> diskStore(std::shared_ptr<Item const> const& itemPtr) const;

  Json toJson(std::shared_ptr<Item const> const& itemPtr) const;

  bool hasItem(String const& itemName) const;
  [[nodiscard]]
  std::expected<ItemType, StarException> itemType(String const& itemName) const;
  // Friendly name here can be different than the final friendly name, as it
  // can be modified by custom config or builder scripts.
  [[nodiscard]]
  std::expected<String, StarException> itemFriendlyName(String const& itemName) const;
  [[nodiscard]]
  std::expected<StringSet, StarException> itemTags(String const& itemName) const;

  // Generate an item config for the given itemName, parameters, level and seed.
  // Level and seed are used by generation in some item types, and may be stored as part
  // of the unique item data or may be ignored.
  [[nodiscard]]
  std::expected<ItemConfig, StarException> itemConfig(String const& itemName, Json parameters, std::optional<float> level = {}, std::optional<std::uint64_t> seed = {}) const;

  // Returns the path to the item's json file in the assets.
  [[nodiscard]]
  std::expected<std::optional<String>, StarException> itemFile(String const& itemName) const;

  // Generates the config for the given item descriptor and then loads the item
  // from the appropriate factory.  If there is a problem instantiating the
  // item, will return a default item instead.  If item is passed a null
  // ItemDescriptor, it will return a null pointer.
  // The returned item pointer will be shared. Either call ->clone() or use item() instead for a copy.
  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> itemShared(ItemDescriptor descriptor, std::optional<float> level = {}, std::optional<std::uint64_t> seed = {}) const;
  // Same as itemShared, but makes a copy instead. Does not cache.
  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> item(ItemDescriptor descriptor, std::optional<float> level = {}, std::optional<std::uint64_t> seed = {}, bool ignoreInvalid = false) const;


  bool hasRecipeToMake(ItemDescriptor const& item) const;
  bool hasRecipeToMake(ItemDescriptor const& item, StringSet const& allowedTypes) const;

  HashSet<ItemRecipe> recipesForOutputItem(String itemName) const;

  HashSet<ItemRecipe> recipesFromBagContents(List<std::shared_ptr<Item>> const& bag, StringMap<std::uint64_t> const& availableCurrencies) const;
  HashSet<ItemRecipe> recipesFromBagContents(HashMap<ItemDescriptor, std::uint64_t> const& bag, StringMap<std::uint64_t> const& availableCurrencies) const;

  HashSet<ItemRecipe> recipesFromBagContents(List<std::shared_ptr<Item>> const& bag, StringMap<std::uint64_t> const& availableCurrencies, StringSet const& allowedTypes) const;
  HashSet<ItemRecipe> recipesFromBagContents(HashMap<ItemDescriptor, std::uint64_t> const& bag, StringMap<std::uint64_t> const& availableCurrencies, StringSet const& allowedTypes) const;

  std::uint64_t maxCraftableInBag(List<std::shared_ptr<Item>> const& bag, StringMap<std::uint64_t> const& availableCurrencies, ItemRecipe const& recipe) const;
  std::uint64_t maxCraftableInBag(HashMap<ItemDescriptor, std::uint64_t> const& bag, StringMap<std::uint64_t> const& availableCurrencies, ItemRecipe const& recipe) const;

  ItemRecipe getPreciseRecipeForMaterials(String const& group, List<std::shared_ptr<Item>> const& bag, StringMap<std::uint64_t> const& availableCurrencies) const;

  [[nodiscard]]
  std::expected<ItemRecipe, StarException> parseRecipe(Json const& config) const;

  HashSet<ItemRecipe> const& allRecipes() const;
  HashSet<ItemRecipe> allRecipes(StringSet const& types) const;

  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> applyAugment(std::shared_ptr<Item> const item, AugmentItem& augment) const;
  [[nodiscard]]
  std::expected<bool, StarException> ageItem(std::shared_ptr<Item>& item, double aging) const;

  List<String> allItems() const;

private:
  explicit ItemDatabase(Root& root);
  struct ItemData {
    ItemType type;
    String name;
    String friendlyName;
    StringSet itemTags;
    StringList agingScripts;
    std::optional<String> assetsConfig;
    JsonObject customConfig;
    String directory;
    String filename;
  };

  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> createItem(ItemType type, ItemConfig const& config) const;
  [[nodiscard]]
  std::expected<ItemDescriptor, StarException> loadDescriptor(Json const& store) const;
  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> tryCreateItem(ItemDescriptor const& descriptor, std::optional<float> level = {}, std::optional<std::uint64_t> seed = {}, bool ignoreInvalid = false) const;

  [[nodiscard]]
  std::expected<std::reference_wrapper<ItemData const>, StarException> itemData(String const& name) const;
  [[nodiscard]]
  std::expected<ItemRecipe, StarException> makeRecipe(List<ItemDescriptor> inputs, ItemDescriptor output, float duration, StringSet groups) const;

  [[nodiscard]]
  std::expected<void, StarException> addItemSet(ItemType type, String const& extension);
  [[nodiscard]]
  std::expected<void, StarException> addObjectDropItem(String const& objectPath, Json const& objectConfig);

  [[nodiscard]]
  std::expected<void, StarException> scanItems();
  [[nodiscard]]
  std::expected<void, StarException> addObjectItems();
  [[nodiscard]]
  std::expected<void, StarException> scanRecipes();
  [[nodiscard]]
  std::expected<void, StarException> addBlueprints();
  [[nodiscard]]
  std::expected<void, StarException> addCodexes();
  Root& m_root;

  StringMap<ItemData> m_items;
  HashSet<ItemRecipe> m_recipes;

  mutable std::recursive_mutex m_luaMutex;
  std::shared_ptr<LuaRoot> m_luaRoot;
  std::shared_ptr<Rebuilder> m_rebuilder;

  typedef std::tuple<ItemDescriptor, std::optional<float>, std::optional<std::uint64_t>> ItemCacheEntry;

  mutable std::mutex m_cacheMutex;
  mutable HashTtlCache<ItemCacheEntry, std::shared_ptr<Item>> m_itemCache;
};

template <typename ItemT>
[[nodiscard]]
std::expected<bool, StarException> ItemDatabase::loadItem(ItemDescriptor const& descriptor, std::shared_ptr<ItemT>& itemPtr) const {
  if (descriptor.isNull()) {
    if (itemPtr) {
      itemPtr.reset();
      return true;
    }
  } else {
    if (!itemPtr || !itemPtr->matches(descriptor, true)) {
      auto loaded = item(descriptor);
      if (!loaded)
        return std::unexpected(std::move(loaded.error()));
      itemPtr = as<ItemT>(*loaded);
      return true;
    } else if (itemPtr->count() != descriptor.count()) {
      itemPtr->setCount(descriptor.count());
      return true;
    }
  }
  return false;
}

template <typename ItemT>
[[nodiscard]]
std::expected<bool, StarException> ItemDatabase::diskLoad(Json const& diskStore, std::shared_ptr<ItemT>& itemPtr) const {
  auto descriptor = loadDescriptor(diskStore);
  if (!descriptor)
    return false;
  auto loaded = loadItem(*descriptor, itemPtr);
  if (!loaded)
    return false;
  return loaded;
}

}

export module star.item_database;

export namespace Star {
  using ::Star::ItemDatabaseExceptionTag;
  using ::Star::ItemDatabaseException;
  using ::Star::ItemType;
  using ::Star::ItemTypeNames;
  using ::Star::RecipeDatabase;
  using ::Star::AugmentItem;
  using ::Star::ItemDatabase;
  using ::Star::Rebuilder;
}
