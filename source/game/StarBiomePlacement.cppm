module;
import std;
import star.string;
import star.format;
import star.json;
import star.bi_map;
import star.random;
import star.perlin;
import star.weighted_pool;
import star.id_map;
import star.set;
import star.casting;
import star.data_stream;
import star.game_types;
import star.list;
import star.vector;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.strong_typedef;


import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.interaction_types;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_damage;
import star.inspectable_entity;
import star.plant;

import star.plant_database;

namespace Star {
class Root;
class Assets;

struct BiomeExceptionTag {
  static constexpr std::string_view name() { return "BiomeException"; }
};
using BiomeException = StarError<BiomeExceptionTag, StarException>;

using TreePair = std::pair<TreeVariant, TreeVariant>;

// Weighted pairs of object name / parameters.
using ObjectPool = WeightedPool<std::pair<String, Json>>;

template <typename BaseType>
struct TreasureBoxSetWrapper : BaseType {
  using BaseType::BaseType;

  TreasureBoxSetWrapper() : BaseType() {}

  TreasureBoxSetWrapper(TreasureBoxSetWrapper const& nt) : BaseType(nt) {}

  TreasureBoxSetWrapper(TreasureBoxSetWrapper&& nt) : BaseType(std::move(nt)) {}

  explicit TreasureBoxSetWrapper(BaseType const& bt) : BaseType(bt) {}

  explicit TreasureBoxSetWrapper(BaseType&& bt) : BaseType(std::move(bt)) {}

  TreasureBoxSetWrapper& operator=(TreasureBoxSetWrapper const& rhs) {
    BaseType::operator=(rhs);
    return *this;
  }

  TreasureBoxSetWrapper& operator=(TreasureBoxSetWrapper&& rhs) {
    BaseType::operator=(std::move(rhs));
    return *this;
  }

  template <class Arg>
  TreasureBoxSetWrapper& operator=(Arg&& other) {
    static_assert(
      std::is_base_of_v<BaseType, std::decay_t<Arg>> == false
        || std::is_same_v<TreasureBoxSetWrapper, std::decay_t<Arg>>,
      "TreasureBoxSet can not implicitly be assigned from String-derived classes or strong String typedefs");

    BaseType::operator=(std::forward<Arg>(other));
    return *this;
  }
};
using TreasureBoxSet = TreasureBoxSetWrapper<String>;
template <typename BaseType>
struct MicroDungeonNamesWrapper : BaseType {
  using BaseType::BaseType;

  MicroDungeonNamesWrapper() : BaseType() {}

  MicroDungeonNamesWrapper(MicroDungeonNamesWrapper const& nt) : BaseType(nt) {}

  MicroDungeonNamesWrapper(MicroDungeonNamesWrapper&& nt) : BaseType(std::move(nt)) {}

  explicit MicroDungeonNamesWrapper(BaseType const& bt) : BaseType(bt) {}

  explicit MicroDungeonNamesWrapper(BaseType&& bt) : BaseType(std::move(bt)) {}

  MicroDungeonNamesWrapper& operator=(MicroDungeonNamesWrapper const& rhs) {
    BaseType::operator=(rhs);
    return *this;
  }

  MicroDungeonNamesWrapper& operator=(MicroDungeonNamesWrapper&& rhs) {
    BaseType::operator=(std::move(rhs));
    return *this;
  }

  template <class Arg>
  MicroDungeonNamesWrapper& operator=(Arg&& other) {
    static_assert(
      std::is_base_of_v<BaseType, std::decay_t<Arg>> == false
        || std::is_same_v<MicroDungeonNamesWrapper, std::decay_t<Arg>>,
      "MicroDungeonNames can not implicitly be assigned from StringSet-derived classes or strong StringSet typedefs");

    BaseType::operator=(std::forward<Arg>(other));
    return *this;
  }
};
using MicroDungeonNames = MicroDungeonNamesWrapper<StringSet>;

using BiomeItem = std::variant<GrassVariant, BushVariant, TreePair, ObjectPool, TreasureBoxSet, MicroDungeonNames>;
[[nodiscard]]
std::expected<BiomeItem, StarException> variantToBiomeItem(Assets const& assets, Json const& store);
[[nodiscard]]
std::expected<Json, StarException> variantFromBiomeItem(BiomeItem const& biomeItem);

enum class BiomePlacementArea { Surface, Underground };
enum class BiomePlacementMode { Floor, Ceiling, Background, Ocean };
inline constexpr auto BiomePlacementModeNames = sortedEnumNames(std::to_array<std::pair<BiomePlacementMode, std::string_view>>({
  {BiomePlacementMode::Floor, "floor"},
  {BiomePlacementMode::Ceiling, "ceiling"},
  {BiomePlacementMode::Background, "background"},
  {BiomePlacementMode::Ocean, "ocean"}
}));

struct BiomeItemPlacement {
  BiomeItemPlacement(BiomeItem item, Vec2I position, float priority);

  // Orders by priority
  bool operator<(BiomeItemPlacement const& rhs) const;

  BiomeItem item;
  Vec2I position;
  float priority;
};

class BiomeItemDistribution {
public:
  struct PeriodicWeightedItem {
    BiomeItem item;
    PerlinF weight;
  };

  [[nodiscard]]
  static std::expected<std::optional<BiomeItem>, StarException> createItem(Root& root, Json const& itemSettings, RandomSource& rand, float biomeHueShift);

  BiomeItemDistribution();
  [[nodiscard]]
  static std::expected<BiomeItemDistribution, StarException> create(Root& root, Json const& config, std::uint64_t seed, float biomeHueShift = 0.0f);
  [[nodiscard]]
  static std::expected<BiomeItemDistribution, StarException> create(Assets const& assets, Json const& store);

  [[nodiscard]]
  std::expected<Json, StarException> toJson() const;

  BiomePlacementMode mode() const;
  List<BiomeItem> allItems() const;

  // Returns the best BiomeItem for this position out of the weighted item set,
  // if the density function specifies that an item should go in this position.
  [[nodiscard]]
  std::expected<std::optional<BiomeItemPlacement>, StarException> itemToPlace(int x, int y) const;

private:
  enum class DistributionType {
    // Pure random distribution
    Random,
    // Uses perlin noise to morph a periodic function into a less predictable
    // periodic clumpy noise.
    Periodic
  };
  inline static constexpr auto DistributionTypeNames = sortedEnumNames(std::to_array<std::pair<DistributionType, std::string_view>>({
    {DistributionType::Random, "random"},
    {DistributionType::Periodic, "periodic"}
  }));

  BiomePlacementMode m_mode;
  DistributionType m_distribution;
  float m_priority;

  // Used if the distribution type is Random

  float m_blockProbability;
  std::uint64_t m_blockSeed;
  List<BiomeItem> m_randomItems;

  // Used if the distribution type is Periodic

  PerlinF m_densityFunction;
  PerlinF m_modulusDistortion;
  int m_modulus;
  int m_modulusOffset;
  // Pairs items with a periodic weight.  Weight will vary over the space of
  // the distribution, If multiple items are present, this can be used to
  // select one of the items (with the highest weight) out of a list of items,
  // causing items to be grouped spatially in a way determined by the shape of
  // each weight function.
  List<std::pair<BiomeItem, PerlinF>> m_weightedItems;
};

}

export module star.biome_placement;

export namespace Star {
  using ::Star::BiomeItemDistribution;
  using ::Star::BiomeExceptionTag;
  using ::Star::BiomeException;
  using ::Star::TreePair;
  using ::Star::ObjectPool;
  using ::Star::TreasureBoxSetWrapper;
  using ::Star::TreasureBoxSet;
  using ::Star::MicroDungeonNamesWrapper;
  using ::Star::MicroDungeonNames;
  using ::Star::BiomeItem;
  using ::Star::variantToBiomeItem;
  using ::Star::variantFromBiomeItem;
  using ::Star::BiomePlacementArea;
  using ::Star::BiomePlacementMode;
  using ::Star::BiomePlacementModeNames;
  using ::Star::BiomeItemPlacement;
}
