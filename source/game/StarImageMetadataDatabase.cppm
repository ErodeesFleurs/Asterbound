module;
import star.format;
import std;
import star.json;

import star.rect;
import star.thread;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ordered_map;
import star.lru_cache;
import star.time;
import star.random;
import star.ttl_cache;

import star.vector;
namespace Star {
class Root;


// Caches image size, image spaces, and nonEmptyRegion completely until a
// reload, does not expire cached values in a TTL based way like Assets,
// because they are expensive to compute and cheap to keep around.
class ImageMetadataDatabase {
public:
  explicit ImageMetadataDatabase(Root& root);
  [[nodiscard]] std::expected<Vec2U, StarException> imageSize(AssetPath const& path) const;
  [[nodiscard]] std::expected<List<Vec2I>, StarException> imageSpaces(AssetPath const& path, Vec2F position, float fillLimit, bool flip) const;
  [[nodiscard]] std::expected<RectU, StarException> nonEmptyRegion(AssetPath const& path) const;
  void cleanup() const;

private:
  // Removes image processing directives that don't affect image spaces /
  // non-empty regions.
  static AssetPath filterProcessing(AssetPath const& path);

  [[nodiscard]] std::expected<Vec2U, StarException> calculateImageSize(AssetPath const& path) const;

  // Path, position, fillLimit, and flip
  using SpacesEntry = std::tuple<AssetPath, Vec2I, float, bool>;

  Root& m_root;
  mutable std::mutex m_mutex;
  mutable HashTtlCache<AssetPath, Vec2U> m_sizeCache;
  mutable HashTtlCache<SpacesEntry, List<Vec2I>> m_spacesCache;
  mutable HashTtlCache<AssetPath, RectU> m_regionCache;
};

}

export module star.image_metadata_database;

export namespace Star {
  using ::Star::ImageMetadataDatabase;
}
