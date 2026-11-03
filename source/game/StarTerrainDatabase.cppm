module;

import std;
import star.string;
import star.format;
import star.json;

namespace Star {
class Assets;
class RandomService;

struct TerrainExceptionTag {
  static constexpr std::string_view name() { return "TerrainException"; }
};
using TerrainException = StarError<TerrainExceptionTag, StarException>;

struct TerrainSelectorParameters {
  explicit TerrainSelectorParameters(RandomService& random);
  [[nodiscard]] static std::expected<TerrainSelectorParameters, StarException> create(Json const& v);

  Json toJson() const;

  TerrainSelectorParameters withSeed(std::uint64_t seed) const;
  TerrainSelectorParameters withCommonality(float commonality) const;

  unsigned worldWidth;
  float baseHeight;
  std::uint64_t seed;
  float commonality;
private:
  TerrainSelectorParameters() = default;
};

struct TerrainSelector {
  TerrainSelector(String type, Json config, TerrainSelectorParameters parameters);
  virtual ~TerrainSelector();

  // Returns a float signifying the "solid-ness" of a block, >= 0.0 should be
  // considered solid, < 0.0 should be considered open space.
  [[nodiscard]] virtual std::expected<float, StarException> get(int x, int y) const = 0;

  String type;
  Json config;
  TerrainSelectorParameters parameters;
};

class TerrainDatabase {
public:
  [[nodiscard]] std::expected<std::shared_ptr<TerrainSelector const>, StarException> createSelectorType(String const& type, Json const& config, TerrainSelectorParameters const& parameters) const;

  [[nodiscard]] static std::expected<std::shared_ptr<TerrainDatabase>, StarException> create(std::shared_ptr<Assets const> const& assets, RandomService& random);
private:
  struct ConstructionKey { explicit ConstructionKey() = default; };
public:
  TerrainDatabase(ConstructionKey, RandomService& random);

  [[nodiscard]] std::expected<std::shared_ptr<TerrainSelector const>, StarException> createNamedSelector(String const& name, TerrainSelectorParameters const& parameters) const;
  [[nodiscard]] std::expected<std::shared_ptr<TerrainSelector const>, StarException> constantSelector(float value);

  Json storeSelector(std::shared_ptr<TerrainSelector const> const& selector) const;
  [[nodiscard]] std::expected<std::shared_ptr<TerrainSelector const>, StarException> loadSelector(Json const& store) const;

private:
  struct Config {
    String type;
    Json parameters;
  };

  RandomService& m_random;
  StringMap<Config> m_terrainSelectors;
};

}

export module star.terrain_database;

export namespace Star {
  using ::Star::TerrainExceptionTag;
  using ::Star::TerrainException;
  using ::Star::TerrainSelectorParameters;
  using ::Star::TerrainSelector;
  using ::Star::TerrainDatabase;
}
