module;
import star.memory;
import std;
import star.type_identity;
import star.format;
import star.json;
import star.set;
import star.string;
import star.data_stream;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.bi_map;
import star.game_types;
import star.strong_typedef;

namespace Star {
class Root;
class TileDrawer;
class Item;
class GenericItem;
}

import star.drawable;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;

namespace Star {


struct ItemExceptionTag {
  static constexpr std::string_view name() { return "ItemException"; }
};
using ItemException = StarError<ItemExceptionTag, StarException>;

class Item {
public:
  virtual void const* query_type(TypeKey key) const noexcept { return query_type_of<Item>(this, key); }
  virtual std::string_view dynamic_type_name() const noexcept { return Star::type_name<Item>(); }

  virtual ~Item();

  [[nodiscard]]
  virtual std::expected<std::shared_ptr<Item>, StarException> clone() const = 0;

  // Unique identifying item name
  String name() const;

  // Number of this item that is available.
  std::uint64_t count() const;
  // Sets the new item count, up to a max of the maximum stack size.  If this
  // value is over stack size, returns the overflow.  If 'overfill' is set to
  // true, then will fill past max stack level.
  std::uint64_t setCount(std::uint64_t count, bool overfill = false);

  // Is this item type stackable with the given item type at all?  Base class
  // implementation compares name(), and m_parameters fields and returns true
  // if they are both the same, similarly to matches. Null is not stackable.
  virtual bool stackableWith(std::optional<std::reference_wrapper<Item const>> item) const;
  std::uint64_t maxStack() const;

  // Return how many of the given item could be shifted into this item, taking
  // into acount whether the item is stackable at all, as well as maxStack and
  // the count available. Null contributes no items.
  std::uint64_t couldStack(std::optional<std::reference_wrapper<Item const>> item) const;

  // If the given item is stackable with this one, takes as many from the given
  // item as possible and shifts it into this item's count.  Returns true if
  // any items at all were shifted. Null is left unchanged and returns false.
  bool stackWith(std::optional<std::reference_wrapper<Item>> item);

  // Does this item match the given item or itemDescriptor
  bool matches(ItemDescriptor const& descriptor, bool exactMatch = false) const;
  bool matches(Item const& other, bool exactMatch = false) const;

  // List of itemdescriptors for which the current item could be used in the
  // place of
  // in recipes and the like.
  List<ItemDescriptor> matchingDescriptors() const;

  // If the given number of this item is available, consumes that number and
  // returns true, otherwise returns false.
  bool consume(std::uint64_t count);

  // Take as many of this item as possible up to the given max (default is all)
  // and return the new set.  Implementation uses clone() method.
  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> take(std::uint64_t max = NPos);

  // count() is 0
  bool empty() const;

  // Builds a descriptor out of name(), count(), and m_parameters
  ItemDescriptor descriptor() const;

  String description() const;
  String friendlyName() const;

  Rarity rarity() const;
  std::uint64_t price() const;

  [[nodiscard]]
  virtual std::expected<List<Drawable>, StarException> iconDrawables() const;
  [[nodiscard]]
  virtual std::expected<std::optional<List<Drawable>>, StarException> secondaryDrawables() const;
  virtual bool hasSecondaryDrawables() const;

  [[nodiscard]]
  virtual std::expected<List<Drawable>, StarException> dropDrawables(std::optional<std::reference_wrapper<TileDrawer>> tileDrawer) const;
  String largeImage() const;

  String tooltipKind() const;
  virtual String category() const;

  [[nodiscard]]
  virtual std::expected<String, StarException> pickupSound() const;

  bool twoHanded() const;
  float timeToLive() const;

  List<ItemDescriptor> learnBlueprintsOnPickup() const;
  StringMap<String> collectablesOnPickup() const;

  [[nodiscard]]
  std::expected<List<QuestArcDescriptor>, StarException> pickupQuestTemplates() const;
  [[nodiscard]]
  std::expected<StringSet, StarException> itemTags() const;
  [[nodiscard]]
  std::expected<bool, StarException> hasItemTag(String const& itemTag) const;

  // Return either a parameter given to the item or a config value, if no such
  // parameter exists.
  [[nodiscard]]
  std::expected<Json, StarException> instanceValue(String const& name, Json const& def = Json()) const;
  [[nodiscard]]
  std::expected<Json, StarException> instanceValueOfType(String const& name, Json::Type type, Json const& def = Json()) const;

  // Returns the full set of configuration values merged with parameters
  [[nodiscard]]
  std::expected<Json, StarException> instanceValues() const;

  // Returns just the base config
  Json config() const;

  // Returns just the dynamic parameters
  Json parameters() const;

  static bool itemsEqual(std::optional<std::reference_wrapper<Item const>> a, std::optional<std::reference_wrapper<Item const>> b);

protected:
  explicit Item(Root& root);
  [[nodiscard]]
  std::expected<void, StarException> initialize(Json config, String directory, Json parameters = JsonObject());
  Root& root() const;

  void setMaxStack(std::uint64_t maxStack);
  void setDescription(String const& description);
  void setShortDescription(String const& description);

  void setRarity(Rarity rarity);
  void setPrice(std::uint64_t price);
  // icon drawables are pixels, not tile, based
  [[nodiscard]]
  std::expected<void, StarException> setIconDrawables(List<Drawable> drawables);
  [[nodiscard]]
  std::expected<void, StarException> setSecondaryIconDrawables(std::optional<List<Drawable>> drawables);
  void setTwoHanded(bool twoHanded);
  void setTimeToLive(float timeToLive);

  [[nodiscard]]
  std::expected<void, StarException> setInstanceValue(String const& name, Json const& val);

  String const& directory() const;

private:
  std::reference_wrapper<Root> m_root;
  Json m_config;
  String m_directory;

  String m_name;
  std::uint64_t m_count;
  Json m_parameters;

  std::uint64_t m_maxStack;
  String m_shortDescription;
  String m_description;
  Rarity m_rarity;
  List<Drawable> m_iconDrawables;
  std::optional<List<Drawable>> m_secondaryIconDrawables;
  bool m_twoHanded;
  float m_timeToLive;
  std::uint64_t m_price;
  String m_tooltipKind;
  String m_largeImage;
  String m_category;
  StringSet m_pickupSounds;

  List<ItemDescriptor> m_matchingDescriptors;
  List<ItemDescriptor> m_learnBlueprintsOnPickup;
  StringMap<String> m_collectablesOnPickup;
};

class GenericItem : public Item {
public:
  void const* query_type(TypeKey key) const noexcept override { return query_type_of<GenericItem, Item>(this, key); }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<GenericItem>(); }

  [[nodiscard]]
  static std::expected<std::shared_ptr<GenericItem>, StarException> create(Root& root, Json const& config, String const& directory, Json const& parameters);
  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> clone() const override;
private:
  explicit GenericItem(Root& root) : Item(root) {}
};

inline std::uint64_t itemSafeCount(std::shared_ptr<Item> const& item) {
  return item ? item->count() : 0;
}

inline bool itemSafeTwoHanded(std::shared_ptr<Item> const& item) {
  return item && item->twoHanded();
}

inline bool itemSafeOneHanded(std::shared_ptr<Item> const& item) {
  return item && !item->twoHanded();
}

inline ItemDescriptor itemSafeDescriptor(std::shared_ptr<Item> const& item) {
  return item ? item->descriptor() : ItemDescriptor();
}
}


export module star.item;

export namespace Star {
  using ::Star::Root;
  using ::Star::TileDrawer;
  using ::Star::Item;
  using ::Star::GenericItem;
  using ::Star::ItemExceptionTag;
  using ::Star::ItemException;
  using ::Star::itemSafeCount;
  using ::Star::itemSafeTwoHanded;
  using ::Star::itemSafeOneHanded;
  using ::Star::itemSafeDescriptor;
}

