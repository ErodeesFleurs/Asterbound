module;
import std;
import star.string;
import star.format;
import star.json;
import star.bi_map;
import star.perlin;
import star.weighted_pool;
import star.list;
import star.vector;
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

struct TreasureBoxSet {
  String name;
};

struct MicroDungeonNames {
  StringSet names;
};

using BiomeItem = std::variant<GrassVariant, BushVariant, TreePair, ObjectPool, TreasureBoxSet, MicroDungeonNames>;
[[nodiscard]]
std::expected<BiomeItem, StarException> variant_to_biome_item(Assets const& assets, Json const& store);
[[nodiscard]]
std::expected<Json, StarException> variant_from_biome_item(BiomeItem const& item);

enum class BiomePlacementArea { Surface, Underground };
enum class BiomePlacementMode { Floor, Ceiling, Background, Ocean };
inline constexpr auto biome_placement_mode_names = sorted_enum_names(std::to_array<std::pair<BiomePlacementMode, std::string_view>>({
  {BiomePlacementMode::Floor, "floor"},
  {BiomePlacementMode::Ceiling, "ceiling"},
  {BiomePlacementMode::Background, "background"},
  {BiomePlacementMode::Ocean, "ocean"}
}));

struct BiomeItemPlacement {
  // Orders by priority
  bool operator<(BiomeItemPlacement const& rhs) const { return priority < rhs.priority; }

  BiomeItem item;
  Vec2I position;
  float priority = 0.0f;
};

class BiomeItemDistribution {
public:
  [[nodiscard]]
  static std::expected<BiomeItemDistribution, StarException> create(Root& root, Json const& config, std::uint64_t seed, float biome_hue_shift = 0.0f);
  [[nodiscard]]
  static std::expected<BiomeItemDistribution, StarException> create(Assets const& assets, Json const& store);

  [[nodiscard]]
  std::expected<Json, StarException> to_json() const;

  BiomePlacementMode mode() const { return m_mode; }
  float priority() const { return m_priority; }
  std::size_t size() const;

  [[nodiscard]] std::optional<std::reference_wrapper<TreeVariant const>> first_tree() const &;
  std::optional<std::reference_wrapper<TreeVariant const>> first_tree() && = delete;
  std::optional<std::reference_wrapper<TreeVariant const>> first_tree() const && = delete;

  // Borrows a candidate for immediate use; retained placements own their item.
  [[nodiscard]] std::expected<std::optional<std::reference_wrapper<BiomeItem const>>, StarException> item_at(int x, int y) const &;
  std::expected<std::optional<std::reference_wrapper<BiomeItem const>>, StarException> item_at(int, int) && = delete;
  std::expected<std::optional<std::reference_wrapper<BiomeItem const>>, StarException> item_at(int, int) const && = delete;
  [[nodiscard]] std::expected<std::optional<BiomeItemPlacement>, StarException> item_to_place(int x, int y) const;

private:
  enum class DistributionType {
    // Pure random distribution
    Random,
    // Uses perlin noise to morph a periodic function into a less predictable
    // periodic clumpy noise.
    Periodic
  };
  inline static constexpr auto distribution_type_names = sorted_enum_names(std::to_array<std::pair<DistributionType, std::string_view>>({
    {DistributionType::Random, "random"},
    {DistributionType::Periodic, "periodic"}
  }));

  struct PeriodicWeightedItem {
    PeriodicWeightedItem(BiomeItem&& value, PerlinF&& noise)
      : item(std::move(value)), weight(std::move(noise)) {}
    PeriodicWeightedItem(BiomeItem&& value, PerlinF::Parameters const& parameters, std::uint64_t seed)
      : item(std::move(value)), weight(parameters, seed) {}

    BiomeItem item;
    PerlinF weight;
  };

  BiomePlacementMode m_mode = BiomePlacementMode::Floor;
  DistributionType m_distribution = DistributionType::Random;
  float m_priority = 0.0f;

  // Used if the distribution type is Random

  float m_block_probability = 0.0f;
  std::uint64_t m_block_seed = 0;
  List<BiomeItem> m_random_items;

  // Used if the distribution type is Periodic

  PerlinF m_density_function;
  PerlinF m_modulus_distortion;
  int m_modulus = 1;
  int m_modulus_offset = 0;
  List<PeriodicWeightedItem> m_weighted_items;
};

}

export module star.biome_placement;

export namespace Star {
  using ::Star::BiomeItemDistribution;
  using ::Star::BiomeExceptionTag;
  using ::Star::BiomeException;
  using ::Star::TreePair;
  using ::Star::ObjectPool;
  using ::Star::TreasureBoxSet;
  using ::Star::MicroDungeonNames;
  using ::Star::BiomeItem;
  using ::Star::variant_to_biome_item;
  using ::Star::variant_from_biome_item;
  using ::Star::BiomePlacementArea;
  using ::Star::BiomePlacementMode;
  using ::Star::biome_placement_mode_names;
  using ::Star::BiomeItemPlacement;
}
