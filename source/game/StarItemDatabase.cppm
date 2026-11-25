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

namespace Star {

class Root;
class Player;
class AugmentItem;
class Assets;

class Rebuilder;
class LuaRoot;

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
inline constexpr auto item_type_names = sorted_enum_names(std::to_array<std::pair<ItemType, std::string_view>>({
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

template <typename T>
concept loadable_item = std::derived_from<T, Item> && std::same_as<T, std::remove_cv_t<T>>
    && requires(std::shared_ptr<Item> value) { { as<T>(std::move(value)) } -> std::same_as<std::shared_ptr<T>>; };

class ItemDatabase {
  class ConstructionKey {
    friend class ItemDatabase;
    ConstructionKey() = default;

  public:
    ConstructionKey(ConstructionKey const&) = default;
  };

public:
  ItemDatabase(ConstructionKey, Root& root);

  // During item loading, the ItemDatabase takes the ItemDescriptor and
  // produces a set of things from it:
  struct ItemConfig {
    // The relative path in assets to the base config
    String directory;

    // The immutable catalog config, or a fresh replacement returned by the builder.
    Json config;

    // The parameters from the ItemDescriptor, also possibly modified during
    // loading. The resulting parameters will be stored as part of the item's
    // new ItemDescriptor.
    Json parameters;
  };

  static std::uint64_t get_count_of_item(RecipeStock const& bag, ItemDescriptor const& item, bool exact_match = false);
  static RecipeStock normalize_bag(List<std::shared_ptr<Item>> const& bag);
  [[nodiscard]] std::expected<bool, StarException> craft_recipe(Player& player, ItemRecipe const& recipe, std::uint64_t batches) const;
  static bool can_make_recipe(ItemRecipe const& recipe, RecipeStock const& available_ingredients, StringMap<std::uint64_t> const& available_currencies);
  static HashSet<ItemRecipe> recipes_from_subset(RecipeStock const& normalized_bag, StringMap<std::uint64_t> const& available_currencies, HashSet<ItemRecipe> const& subset, StringSet const& allowed_types);
  [[nodiscard]]
  static std::expected<String, StarException> gui_filter_string(std::shared_ptr<Item> const& item);

  [[nodiscard]]
  static std::expected<std::shared_ptr<ItemDatabase>, StarException> create(Root& root);

  void cleanup();

  // Reuse an exact match, adjusting its count if necessary. A non-castable
  // replacement clears the slot. On construction failure the old slot survives.
  template <loadable_item ItemT>
  [[nodiscard]]
  std::expected<bool, StarException> load_item(ItemDescriptor const& descriptor, std::shared_ptr<ItemT>& slot) const;

  template <loadable_item ItemT>
  [[nodiscard]]
  std::expected<bool, StarException> disk_load(Json const& store, std::shared_ptr<ItemT>& slot) const;

  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> disk_load(Json const& store) const;
  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> from_json(Json const& spec) const;

  [[nodiscard]]
  std::expected<Json, StarException> disk_store(std::shared_ptr<Item const> const& item) const;

  Json to_json(std::shared_ptr<Item const> const& item) const;

  bool has_item(String const& item_name) const;
  [[nodiscard]]
  std::expected<ItemType, StarException> item_type(String const& item_name) const;
  // Friendly name here can be different than the final friendly name, as it
  // can be modified by custom config or builder scripts.
  [[nodiscard]]
  std::expected<String, StarException> item_friendly_name(String const& item_name) const;
  [[nodiscard]]
  std::expected<StringSet, StarException> item_tags(String const& item_name) const;

  // Generate an item config for the given item name, parameters, level and seed.
  // Level and seed are used by generation in some item types, and may be stored as part
  // of the unique item data or may be ignored.
  [[nodiscard]]
  std::expected<ItemConfig, StarException> item_config(String const& item_name, Json parameters, std::optional<float> level = {}, std::optional<std::uint64_t> seed = {}) const;

  // Returns the path to the item's json file in the assets.
  [[nodiscard]]
  std::expected<std::optional<String>, StarException> item_file(String const& item_name) const;

  // Generates the config for the given item descriptor and then loads the item
  // from the appropriate factory.  If there is a problem instantiating the
  // item, will return a default item instead.  If item is passed a null
  // ItemDescriptor, it will return a null pointer.
  // The returned item pointer will be shared. Either call ->clone() or use item() instead for a copy.
  // Unseeded requests whose results contain a seed are not cached.
  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> item_shared(ItemDescriptor descriptor, std::optional<float> level = {}, std::optional<std::uint64_t> seed = {}) const;
  // Like item_shared, but constructs a fresh item instead of using the cache.
  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> item(ItemDescriptor descriptor, std::optional<float> level = {}, std::optional<std::uint64_t> seed = {}, bool ignore_invalid = false) const;

  HashSet<ItemRecipe> recipes_for_output_item(String const& item_name) const;

  HashSet<ItemRecipe> recipes_from_bag_contents(RecipeStock const& bag, StringMap<std::uint64_t> const& available_currencies, StringSet const& allowed_types) const;

  static std::uint64_t max_craftable_in_bag(RecipeStock const& bag, StringMap<std::uint64_t> const& available_currencies, ItemRecipe const& recipe);

  ItemRecipe get_precise_recipe_for_materials(String const& group, List<std::shared_ptr<Item>> const& bag, StringMap<std::uint64_t> const& available_currencies) const;

  [[nodiscard]]
  std::expected<ItemRecipe, StarException> parse_recipe(Json const& config) const;

  HashSet<ItemRecipe> const& all_recipes() const;
  HashSet<ItemRecipe> all_recipes(StringSet const& types) const;

  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> apply_augment(std::shared_ptr<Item> const& item, AugmentItem& augment) const;
  [[nodiscard]]
  std::expected<bool, StarException> age_item(std::shared_ptr<Item>& item, double aging) const;

  List<String> all_items() const;

private:
  struct ItemData {
    ItemType type;
    String friendly_name;
    StringSet item_tags;
    StringList aging_scripts;
    Json config;
    String directory;
    String source_path;
    bool asset_config = false;
  };

  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> create_item(ItemType type, ItemConfig const& config) const;
  [[nodiscard]]
  std::expected<ItemDescriptor, StarException> load_descriptor(Json const& store) const;
  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> try_create_item(ItemDescriptor const& descriptor, std::optional<float> level = {}, std::optional<std::uint64_t> seed = {}, bool ignore_invalid = false) const;

  [[nodiscard]]
  std::expected<std::shared_ptr<ItemData const>, StarException> item_data(String const& name) const;
  [[nodiscard]]
  std::expected<ItemConfig, StarException> build_item_config(ItemData const& data, Json parameters, std::optional<float> level, std::optional<std::uint64_t> seed) const;

  using ItemPaths = std::ranges::subrange<CaseInsensitiveStringSet::const_iterator>;

  [[nodiscard]]
  std::expected<void, StarException> add_item_set(Assets const& assets, ItemType type, ItemPaths const& files);
  [[nodiscard]]
  std::expected<void, StarException> add_object_drop_item(Assets const& assets, String const& object_path, Json const& object_config);

  [[nodiscard]]
  std::expected<void, StarException> scan_items();
  [[nodiscard]]
  std::expected<void, StarException> add_object_items();
  [[nodiscard]]
  std::expected<void, StarException> scan_recipes();
  [[nodiscard]]
  std::expected<void, StarException> add_blueprints();
  [[nodiscard]]
  std::expected<void, StarException> add_codexes();
  Root& m_root;

  // Retain an immutable entry across reentrant Lua without borrowing flat-map storage.
  StringMap<std::shared_ptr<ItemData const>> m_items;
  HashSet<ItemRecipe> m_recipes;

  mutable std::recursive_mutex m_lua_mutex;
  std::shared_ptr<LuaRoot> m_lua_root;
  std::shared_ptr<Rebuilder> m_rebuilder;

  using ItemCacheEntry = std::tuple<ItemDescriptor, std::optional<float>, std::optional<std::uint64_t>>;

  mutable std::mutex m_cache_mutex;
  mutable HashTtlCache<ItemCacheEntry, std::shared_ptr<Item>> m_item_cache;
};

template <loadable_item ItemT>
[[nodiscard]]
std::expected<bool, StarException> ItemDatabase::load_item(ItemDescriptor const& descriptor, std::shared_ptr<ItemT>& slot) const {
  if (descriptor.isNull())
    return static_cast<bool>(std::exchange(slot, {}));
  if (slot && slot->matches(descriptor, true)) {
    auto previous = slot->count();
    if (previous == descriptor.count())
      return false;
    slot->set_count(descriptor.count());
    return previous != slot->count();
  }
  auto loaded = item(descriptor);
  if (!loaded)
    return std::unexpected(std::move(loaded.error()));
  auto replacement = as<ItemT>(std::move(*loaded));
  bool changed = slot != replacement;
  slot = std::move(replacement);
  return changed;
}

template <loadable_item ItemT>
[[nodiscard]]
std::expected<bool, StarException> ItemDatabase::disk_load(Json const& store, std::shared_ptr<ItemT>& slot) const {
  if (store.is_null())
    return static_cast<bool>(std::exchange(slot, {}));
  auto descriptor = load_descriptor(store);
  if (!descriptor)
    return std::unexpected(std::move(descriptor.error()));
  return load_item(*descriptor, slot);
}

}

export module star.item_database;

export namespace Star {
  using ::Star::ItemType;
  using ::Star::item_type_names;
  using ::Star::AugmentItem;
  using ::Star::ItemDatabase;
  using ::Star::Rebuilder;
}
