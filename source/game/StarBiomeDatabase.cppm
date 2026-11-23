module;
import std;
import star.format;
import star.string;
import star.json;
import star.weather_types;
import star.sky_types;

namespace Star {
class Root;

struct Biome;

class BiomeDatabase {
  struct ConstructionKey { explicit ConstructionKey() = default; };

public:
  BiomeDatabase(ConstructionKey, Root& root) : m_root(root) {}
  [[nodiscard]]
  static std::expected<std::shared_ptr<BiomeDatabase>, StarException> create(Root& root);

  StringList biome_names() const;

  [[nodiscard]]
  std::expected<float, StarException> biome_hue_shift(String const& biome_name, std::uint64_t seed) const;
  [[nodiscard]]
  std::expected<bool, StarException> biome_has_weather(String const& biome_name) const;
  [[nodiscard]]
  std::expected<WeatherPool, StarException> biome_weathers(String const& biome_name, std::uint64_t seed, float threat_level) const;
  [[nodiscard]]
  std::expected<bool, StarException> biome_is_airless(String const& biome_name) const;
  [[nodiscard]]
  std::expected<SkyColoring, StarException> biome_sky_coloring(String const& biome_name, std::uint64_t seed) const;
  [[nodiscard]]
  std::expected<String, StarException> biome_friendly_name(String const& biome_name) const;
  [[nodiscard]]
  std::expected<StringList, StarException> biome_status_effects(String const& biome_name) const;
  [[nodiscard]]
  std::expected<StringList, StarException> biome_ores(String const& biome_name, float threat_level) const;

  StringList weather_names() const;
  [[nodiscard]]
  std::expected<WeatherType, StarException> weather_type(String const& weather_name) const;

  [[nodiscard]]
  std::expected<std::shared_ptr<Biome>, StarException> create_biome(String const& biome_name, std::uint64_t seed, float vertical_mid_point, float threat_level) const;

  [[nodiscard]]
  std::expected<Json, StarException> biome_config(String const& biome_name) const;

private:
  Root& m_root;
  struct Config {
    String path;
    Json parameters;
  };
  using ConfigMap = StringMap<Config>;

  ConfigMap m_biomes;
  ConfigMap m_weathers;
};

}

export module star.biome_database;

export namespace Star {
  using ::Star::Biome;
  using ::Star::BiomeDatabase;
}
