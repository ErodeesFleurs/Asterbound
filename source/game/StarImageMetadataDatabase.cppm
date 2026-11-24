module;
import std;
import star.asset_path;
import star.format;
import star.image;
import star.list;
import star.rect;
import star.ttl_cache;
import star.vector;
namespace Star {
class Root;

// Entries expire during cleanup; callers receive owning values.
class ImageMetadataDatabase {
public:
  explicit ImageMetadataDatabase(Root& root);
  [[nodiscard]] std::expected<Vec2U, StarException> image_size(AssetPath const& path) const;
  [[nodiscard]] std::expected<List<Vec2I>, StarException> image_spaces(AssetPath const& path, Vec2F position, float fill_limit, bool flip) const;
  [[nodiscard]] std::expected<RectU, StarException> non_empty_region(AssetPath const& path) const;
  void cleanup() const;

private:
  static std::optional<AssetPath> filter_processing(AssetPath const& path);
  using ImageLayout = std::pair<Vec2U, PixelFormat>;
  [[nodiscard]] std::expected<ImageLayout, StarException> cached_image_layout(AssetPath const& path) const;
  [[nodiscard]] std::expected<ImageLayout, StarException> calculate_image_size(AssetPath const& path) const;

  using SpacesEntry = std::tuple<AssetPath, Vec2F, float, bool>;

  Root& m_root;
  mutable std::mutex m_mutex;
  mutable HashTtlCache<AssetPath, ImageLayout> m_size_cache;
  mutable HashTtlCache<SpacesEntry, List<Vec2I>> m_spaces_cache;
  mutable HashTtlCache<AssetPath, RectU> m_region_cache;
};

}

export module star.image_metadata_database;

export namespace Star {
  using ::Star::ImageMetadataDatabase;
}
