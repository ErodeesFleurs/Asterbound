module;
import star.format;
import std;
import star.string;
import star.json;

import star.color;
import star.list;
import star.data_stream;
import star.directives;
import star.vector;
import star.plant_database;

namespace Star {

class Root;
class ImageMetadataDatabase;

struct ParallaxLayer {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& stream, ParallaxLayer& layer);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& stream, ParallaxLayer const& layer);

  [[nodiscard]] static std::expected<ParallaxLayer, StarException> create(Json const& store);

  Json store() const;

  [[nodiscard]] std::expected<void, StarException> fade_to_sky_color(Color const& sky_color);
  unsigned animation_frame(double epoch_time) const;

  List<String> textures;
  Directives directives;
  unsigned frame_number = 1;
  int frame_offset = 0;
  float animation_cycle = 1.0f;
  float alpha = 1.0f;
  Vec2F parallax_value{1.0f, 1.0f};
  Vec2B repeat{};
  std::optional<float> tile_limit_top;
  std::optional<float> tile_limit_bottom;
  float vertical_origin = 0.0f;
  float z_level = 0.0f;
  Vec2F parallax_offset{};
  String time_of_day_correlation;
  Vec2F speed{};
  bool follows_wind = false;
  std::optional<float> wind_speed_multiplier;
  bool unlit = false;
  bool light_mapped = false;
  float fade_percent = 0.0f;
};
using ParallaxLayers = List<ParallaxLayer>;

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& stream, ParallaxLayer& layer);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& stream, ParallaxLayer const& layer);

// Object managing and rendering the parallax for a World
class Parallax {
  struct ConstructionKey { explicit ConstructionKey() = default; };

public:
  Parallax(ConstructionKey, Root& root) : m_root(root) {}

  [[nodiscard]] static std::expected<std::shared_ptr<Parallax>, StarException> create(Root& root, String const& asset_file,
      std::uint64_t seed, float vertical_origin, float hue_shift,
      std::optional<TreeVariant> parallax_tree_variant = {});
  [[nodiscard]] static std::expected<std::shared_ptr<Parallax>, StarException> create(Root& root, Json const& store);

  [[nodiscard]] std::expected<Json, StarException> store() const;

  [[nodiscard]] std::expected<void, StarException> fade_to_sky_color(Color const& sky_color);
  [[nodiscard]] std::expected<std::shared_ptr<Parallax>, StarException> create_overlay(String const& asset_file) const;

  ParallaxLayers const& layers() const;

private:
  [[nodiscard]] std::expected<void, StarException> build_layer(
      Json const& settings, String const& kind, std::shared_ptr<ImageMetadataDatabase const>& image_metadata);

  Root& m_root;
  std::uint64_t m_seed = 0;
  float m_vertical_origin = 0.0f;
  std::optional<TreeVariant> m_parallax_tree_variant;
  float m_hue_shift = 0.0f;
  String m_image_directory;

  ParallaxLayers m_layers;
};

}

export module star.parallax;

export namespace Star {
  using ::Star::Parallax;
  using ::Star::ParallaxLayer;
  using ::Star::ParallaxLayers;
  using ::Star::operator<<;
  using ::Star::operator>>;
}
