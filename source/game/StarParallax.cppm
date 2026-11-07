module;
import star.format;
import std;
import star.string;
import star.json;

import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.vector;
import star.bi_map;
import star.tile_damage;

import star.plant_database;

namespace Star {

class Root;

struct ParallaxLayer {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, ParallaxLayer& parallaxLayer);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, ParallaxLayer const& parallaxLayer);

  ParallaxLayer();
  [[nodiscard]] static std::expected<ParallaxLayer, StarException> create(Json const& store);

  Json store() const;

  void addImageDirectives(Directives const& newDirectives);
  [[nodiscard]] std::expected<void, StarException> fadeToSkyColor(Color skyColor);

  List<String> textures;
  Directives directives;
  unsigned frameNumber;
  int frameOffset;
  float animationCycle;
  float alpha;
  Vec2F parallaxValue;
  Vec2B repeat;
  std::optional<float> tileLimitTop;
  std::optional<float> tileLimitBottom;
  float verticalOrigin;
  float zLevel;
  Vec2F parallaxOffset;
  String timeOfDayCorrelation;
  Vec2F speed;
  bool followsWind;
  std::optional<float> windSpeedMultiplier;
  bool unlit;
  bool lightMapped;
  float fadePercent;
};
using ParallaxLayers = List<ParallaxLayer>;

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, ParallaxLayer& parallaxLayer);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, ParallaxLayer const& parallaxLayer);

// Object managing and rendering the parallax for a World
class Parallax {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<Parallax>, StarException> create(Root& root, String const& assetFile,
      std::uint64_t seed,
      float verticalOrigin,
      float hueShift,
      std::optional<TreeVariant> parallaxTreeVariant = {});
  [[nodiscard]] static std::expected<std::shared_ptr<Parallax>, StarException> create(Root& root, Json const& store);

  [[nodiscard]] std::expected<Json, StarException> store() const;

  [[nodiscard]] std::expected<void, StarException> fadeToSkyColor(Color const& skyColor);
  [[nodiscard]] std::expected<std::shared_ptr<Parallax>, StarException> createOverlay(String const& assetFile) const;

  ParallaxLayers const& layers() const;

private:
  explicit Parallax(Root& root) : m_root(root) {}
  [[nodiscard]] std::expected<void, StarException> buildLayer(Json const& layerSettings, String const& kind);

  Root& m_root;
  std::uint64_t m_seed;
  float m_verticalOrigin;
  std::optional<TreeVariant> m_parallaxTreeVariant;
  float m_hueShift;

  String m_imageDirectory;

  ParallaxLayers m_layers;
};

}

export module star.parallax;

export namespace Star {
  using ::Star::Parallax;
  using ::Star::ParallaxLayer;
  using ::Star::ParallaxLayers;
}
