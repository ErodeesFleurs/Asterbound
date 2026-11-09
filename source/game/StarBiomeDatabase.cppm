module;
import std;
import star.format;
import star.string;
import star.json;

import star.game_types;
import star.material_types;
import star.random;
import star.weighted_pool;
import star.color;
import star.bi_map;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.animation;
import star.particle;
import star.weather_types;
import star.sky_types;

namespace Star {
class Root;

struct Biome;
struct BiomePlaceables;

class BiomeDatabase {
public:
  [[nodiscard]]
  static std::expected<std::shared_ptr<BiomeDatabase>, StarException> create(Root& root);

  StringList biomeNames() const;

  [[nodiscard]]
  std::expected<float, StarException> biomeHueShift(String const& biomeName, std::uint64_t seed) const;
  [[nodiscard]]
  std::expected<bool, StarException> biomeHasWeather(String const& biomeName) const;
  [[nodiscard]]
  std::expected<WeatherPool, StarException> biomeWeathers(String const& biomeName, std::uint64_t seed, float threatLevel) const;
  [[nodiscard]]
  std::expected<bool, StarException> biomeIsAirless(String const& biomeName) const;
  [[nodiscard]]
  std::expected<SkyColoring, StarException> biomeSkyColoring(String const& biomeName, std::uint64_t seed) const;
  [[nodiscard]]
  std::expected<String, StarException> biomeFriendlyName(String const& biomeName) const;
  [[nodiscard]]
  std::expected<StringList, StarException> biomeStatusEffects(String const& biomeName) const;
  [[nodiscard]]
  std::expected<StringList, StarException> biomeOres(String const& biomeName, float threatLevel) const;

  StringList weatherNames() const;
  [[nodiscard]]
  std::expected<WeatherType, StarException> weatherType(String const& weatherName) const;

  [[nodiscard]]
  std::expected<std::shared_ptr<Biome>, StarException> createBiome(String const& biomeName, std::uint64_t seed, float verticalMidPoint, float threatLevel) const;

  [[nodiscard]]
  std::expected<Json, StarException> biomeConfig(String const& biomeName) const;

private:
  explicit BiomeDatabase(Root& root) : m_root(root) {}
  Root& m_root;
  struct Config {
    String path;
    String name;
    Json parameters;
    Json toJson() const;
  };
  using ConfigMap = StringMap<Config>;

  [[nodiscard]]
  static std::expected<float, StarException> pickHueShiftFromJson(Json source, std::uint64_t seed, String const& key);

  [[nodiscard]]
  std::expected<BiomePlaceables, StarException> readBiomePlaceables(Json const& config, std::uint64_t seed, float biomeHueShift) const;
  [[nodiscard]]
  std::expected<List<std::pair<ModId, float>>, StarException> readOres(Json const& oreDistribution, float threatLevel) const;

  ConfigMap m_biomes;
  ConfigMap m_weathers;
};

}

export module star.biome_database;

export namespace Star {
  using ::Star::Biome;
  using ::Star::BiomeDatabase;
}
