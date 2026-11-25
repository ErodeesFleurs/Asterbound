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
  virtual void query_type(TypeQuery& query) const noexcept { query_type_of<Item>(*this, query); }
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
  std::uint64_t set_count(std::uint64_t count, bool overfill = false);

  // Is this item type stackable with the given item type at all?  Base class
  // implementation compares name(), and m_parameters fields and returns true
  // if they are both the same, similarly to matches. Null is not stackable.
  virtual bool stackable_with(std::optional<std::reference_wrapper<Item const>> item) const;
  std::uint64_t max_stack() const;

  // Return how many of the given item could be shifted into this item, taking
  // into account whether the item is stackable at all, as well as max_stack and
  // the count available. Null contributes no items.
  std::uint64_t could_stack(std::optional<std::reference_wrapper<Item const>> item) const;

  // If the given item is stackable with this one, takes as many from the given
  // item as possible and shifts it into this item's count.  Returns true if
  // any items at all were shifted. Null is left unchanged and returns false.
  bool stack_with(std::optional<std::reference_wrapper<Item>> item);

  // Does this item match the given item or descriptor?
  bool matches(ItemDescriptor const& descriptor, bool exact_match = false) const;
  bool matches(Item const& other, bool exact_match = false) const;

  // If the given number of this item is available, consumes that number and
  // returns true, otherwise returns false.
  bool consume(std::uint64_t count);

  // Take as many of this item as possible up to the given max (default is all)
  // and return the new set.  Implementation uses clone() method.
  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> take(std::uint64_t max = npos);

  // count() is 0
  bool empty() const;

  // Builds a descriptor out of name(), count(), and m_parameters
  ItemDescriptor descriptor() const;

  String description() const;
  String friendly_name() const;

  Rarity rarity() const;
  std::uint64_t price() const;

  [[nodiscard]]
  virtual std::expected<List<Drawable>, StarException> icon_drawables() const;
  [[nodiscard]]
  virtual std::expected<std::optional<List<Drawable>>, StarException> secondary_drawables() const;
  virtual bool has_secondary_drawables() const;

  [[nodiscard]]
  virtual std::expected<List<Drawable>, StarException> drop_drawables(std::optional<std::reference_wrapper<TileDrawer>> tile_drawer) const;
  String large_image() const;

  String tooltip_kind() const;
  virtual String category() const;

  [[nodiscard]]
  virtual std::expected<String, StarException> pickup_sound() const;

  bool two_handed() const;
  float time_to_live() const;

  String const& directory() const&;
  String const& directory() const&& = delete;
  List<ItemDescriptor> const& learn_blueprints_on_pickup() const&;
  List<ItemDescriptor> const& learn_blueprints_on_pickup() const&& = delete;
  StringMap<String> const& collectables_on_pickup() const&;
  StringMap<String> const& collectables_on_pickup() const&& = delete;

  [[nodiscard]]
  std::expected<List<QuestArcDescriptor>, StarException> pickup_quest_templates() const;
  [[nodiscard]]
  std::expected<StringSet, StarException> item_tags() const;
  [[nodiscard]]
  std::expected<bool, StarException> has_item_tag(String const& item_tag) const;

  // Return either a parameter given to the item or a config value, if no such
  // parameter exists.
  [[nodiscard]]
  std::expected<Json, StarException> instance_value(String const& name, Json const& def = Json()) const;
  [[nodiscard]]
  std::expected<Json, StarException> instance_value_of_type(String const& name, Json::Type type, Json const& def = Json()) const;

  // Returns the full set of configuration values merged with parameters
  [[nodiscard]]
  std::expected<Json, StarException> instance_values() const;

  // Returns just the base config
  Json config() const;

  // Returns just the dynamic parameters
  Json parameters() const;

  static bool items_equal(std::optional<std::reference_wrapper<Item const>> a, std::optional<std::reference_wrapper<Item const>> b);

protected:
  explicit Item(Root& root);
  [[nodiscard]]
  std::expected<void, StarException> initialize(Json config, String directory, Json parameters = JsonObject());
  Root& root() const;

  void set_max_stack(std::uint64_t max_stack);
  void set_description(String const& description);
  void set_short_description(String const& description);

  void set_rarity(Rarity rarity);
  void set_price(std::uint64_t price);
  // icon drawables are pixels, not tile, based
  [[nodiscard]]
  std::expected<void, StarException> set_icon_drawables(List<Drawable> drawables);
  [[nodiscard]]
  std::expected<void, StarException> set_secondary_icon_drawables(std::optional<List<Drawable>> drawables);
  void set_two_handed(bool two_handed);
  void set_time_to_live(float time_to_live);

  [[nodiscard]]
  std::expected<void, StarException> set_instance_value(String const& name, Json const& value);

private:
  std::reference_wrapper<Root> m_root;
  Json m_config;
  String m_directory;

  String m_name;
  std::uint64_t m_count;
  Json m_parameters;

  std::uint64_t m_max_stack;
  String m_short_description;
  String m_description;
  Rarity m_rarity;
  List<Drawable> m_icon_drawables;
  std::optional<List<Drawable>> m_secondary_icon_drawables;
  bool m_two_handed;
  float m_time_to_live;
  std::uint64_t m_price;
  String m_tooltip_kind;
  String m_large_image;
  String m_category;
  StringSet m_pickup_sounds;

  List<ItemDescriptor> m_learn_blueprints_on_pickup;
  StringMap<String> m_collectables_on_pickup;
};

class GenericItem : public Item {
  struct ConstructionKey { explicit ConstructionKey() = default; };

public:
  GenericItem(ConstructionKey, Root& root) : Item(root) {}

  void query_type(TypeQuery& query) const noexcept override { query_type_of<GenericItem, Item>(*this, query); }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<GenericItem>(); }

  [[nodiscard]]
  static std::expected<std::shared_ptr<GenericItem>, StarException> create(Root& root, Json const& config, String const& directory, Json const& parameters);
  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> clone() const override;
};

inline std::uint64_t item_safe_count(std::shared_ptr<Item> const& item) {
  return item ? item->count() : 0;
}

inline bool item_safe_two_handed(std::shared_ptr<Item> const& item) {
  return item && item->two_handed();
}

inline bool item_safe_one_handed(std::shared_ptr<Item> const& item) {
  return item && !item->two_handed();
}

inline ItemDescriptor item_safe_descriptor(std::shared_ptr<Item> const& item) {
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
  using ::Star::item_safe_count;
  using ::Star::item_safe_two_handed;
  using ::Star::item_safe_one_handed;
  using ::Star::item_safe_descriptor;
}

