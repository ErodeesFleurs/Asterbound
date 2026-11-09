module;
import std;
import star.string;
import star.list;
import star.format;
import star.json;

import star.parametric_function;
import star.random;
import star.weighted_pool;
import star.data_stream;
import star.bi_map;
import star.item_descriptor;
import star.vector;
import star.game_types;

namespace Star {
class Root;

class World;
class Item;
class ItemBag;
class ItemDatabase;
class ContainerObject;

struct TreasureExceptionTag {
  static constexpr std::string_view name() { return "TreasureException"; }
};
using TreasureException = StarError<TreasureExceptionTag, StarException>;

class TreasureDatabase {
public:
  [[nodiscard]]
  static std::expected<std::shared_ptr<TreasureDatabase>, StarException> create(Root& root);

  StringList treasurePools() const;
  bool isTreasurePool(String const& treasurePool) const;

  StringList treasureChestSets() const;
  bool isTreasureChestSet(String const& treasurePool) const;

  [[nodiscard]]
  std::expected<List<std::shared_ptr<Item>>, StarException> createTreasure(String const& treasurePool, float level) const;
  [[nodiscard]]
  std::expected<List<std::shared_ptr<Item>>, StarException> createTreasure(String const& treasurePool, float level, std::uint64_t seed) const;
  [[nodiscard]]
  std::expected<List<std::shared_ptr<Item>>, StarException> createTreasure(String const& treasurePool, float level, std::uint64_t seed, std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog) const;

  // Adds created treasure to the given ItemBags, does not clear the ItemBag
  // first.  Returns overflow items.
  [[nodiscard]]
  std::expected<List<std::shared_ptr<Item>>, StarException> fillWithTreasure(std::shared_ptr<ItemBag> const& itemBag, String const& treasurePool, float level) const;
  [[nodiscard]]
  std::expected<List<std::shared_ptr<Item>>, StarException> fillWithTreasure(std::shared_ptr<ItemBag> const& itemBag, String const& treasurePool, float level, std::uint64_t seed) const;

  // If the given container does not fit at this position, or if the treasure
  // box set does not have an entry with a minimum level less than the given
  // world threat level, this method will return null.
  [[nodiscard]]
  std::expected<std::shared_ptr<ContainerObject>, StarException> createTreasureChest(World& world, String const& treasureChestSet, Vec2I const& position, Direction direction) const;
  [[nodiscard]]
  std::expected<std::shared_ptr<ContainerObject>, StarException> createTreasureChest(World& world, String const& treasureChestSet, Vec2I const& position, Direction direction, std::uint64_t seed) const;

private:
  explicit TreasureDatabase(Root& root) : m_root(root) {}
  [[nodiscard]]
  std::expected<List<std::shared_ptr<Item>>, StarException> createTreasure(String const& treasurePool, float level, std::uint64_t seed, StringSet visitedPools, std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog) const;

  // Specifies either an item descriptor or the name of a valid treasurepool to
  // be
  // used when an entry is selected in a "fill" or "pool" list
  using TreasureEntry = std::variant<std::monostate, String, ItemDescriptor>;

  struct ItemPool {
    ItemPool();

    // If non-empty, the treasure set is pre-filled with this before selecting
    // from the pool.
    List<TreasureEntry> fill;

    // Weighted pool of items to select from.
    WeightedPool<TreasureEntry> pool;

    // Weighted pool for the number of pool rounds.
    WeightedPool<int> poolRounds;

    // Any item levels that are applied will have a random value
    // from this range added to their level.
    Vec2F levelVariance;

    // When generating more than one item, should we allow each cycle to
    // generate an item that is stackable with a previous item?  This is not to
    // say a stack could actually be formed in an ItemBag, simply that the
    // Item::stackableWith method returns true.
    // Note that this flag does not apply to child pools
    bool allowDuplication;
  };
  using TreasurePool = ParametricTable<float, ItemPool>;

  struct TreasureChest {
    TreasureChest();

    StringList containers;
    String treasurePool;
    float minimumLevel;
  };
  using TreasureChestSet = List<TreasureChest>;

  Root& m_root;
  StringMap<TreasurePool> m_treasurePools;
  StringMap<TreasureChestSet> m_treasureChestSets;
};

}

export module star.treasure;

export namespace Star {
  using ::Star::TreasureExceptionTag;
  using ::Star::TreasureException;
  using ::Star::World;
  using ::Star::Item;
  using ::Star::ItemBag;
  using ::Star::ContainerObject;
  using ::Star::TreasureDatabase;
}
