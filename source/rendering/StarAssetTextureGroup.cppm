module;
import std;
import star.format;
import star.json;

import star.string;
import star.map;
import star.bi_map;
import star.thread;
import star.listener;
import star.image;
import star.ref_ptr;
import star.renderer;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;

namespace Star {
class Root;


// Creates a renderer texture group for textures loaded directly from Assets.
class AssetTextureGroup {
public:
  // Root outlives this group; texture loads resolve its current Assets after reload.
  AssetTextureGroup(Root& root, std::shared_ptr<TextureGroup> textureGroup);

  // Load the given texture into the texture group if it is not loaded, and
  // return the texture pointer.
  [[nodiscard]] std::expected<TexturePtr, StarException> loadTexture(AssetPath const& imagePath);

  // If the texture is loaded and ready, returns the texture pointer, otherwise
  // queues the texture using Assets::tryImage and returns nullptr.
  [[nodiscard]] std::expected<TexturePtr, StarException> tryTexture(AssetPath const& imagePath);

  // Has the texture been loaded?
  bool textureLoaded(AssetPath const& imagePath) const;

  // Frees textures unused for textureTimeout milliseconds. Full scans are
  // spaced by up to one tenth of that timeout, capped at one second.
  // Root reloads clear the group immediately, regardless of that interval.
  void cleanup(std::int64_t textureTimeout);

private:
  // Returns the texture parameters.  If tryTexture is true, then returns none
  // if the texture is not loaded, and queues it, otherwise loads texture
  // immediately
  [[nodiscard]] std::expected<TexturePtr, StarException> loadTexture(AssetPath const& imagePath, bool tryTexture);

  Root& m_root;
  std::shared_ptr<TextureGroup> m_textureGroup;
  HashMap<AssetPath, std::pair<TexturePtr, std::int64_t>> m_textureMap;
  HashMap<std::shared_ptr<Image const>, TexturePtr> m_textureDeduplicationMap;
  std::shared_ptr<TrackerListener> m_reloadTracker;
  ListenerSubscription m_reloadSubscription;
  std::int64_t m_lastCleanup = 0;
};

}

export module star.asset_texture_group;

export namespace Star {
  using ::Star::AssetTextureGroup;
}
