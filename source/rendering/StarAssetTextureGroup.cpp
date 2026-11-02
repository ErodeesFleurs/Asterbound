import std;
import star.format;
import star.json;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.string;
import star.bi_map;
import star.listener;
import star.time;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.version;

import star.image;
import star.renderer;
import star.asset_texture_group;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;


import star.image_metadata_database;

import star.algorithm;
namespace Star {

AssetTextureGroup::AssetTextureGroup(Root& root, std::shared_ptr<TextureGroup> textureGroup)
  : m_root(root), m_textureGroup(std::move(textureGroup)) {
  m_reloadTracker = std::make_shared<TrackerListener>();
  m_reloadSubscription = m_root.registerReloadListener(m_reloadTracker);
}

std::expected<TexturePtr, StarException> AssetTextureGroup::loadTexture(AssetPath const& imagePath) {
  return loadTexture(imagePath, false);
}

std::expected<TexturePtr, StarException> AssetTextureGroup::tryTexture(AssetPath const& imagePath) {
  return loadTexture(imagePath, true);
}

bool AssetTextureGroup::textureLoaded(AssetPath const& imagePath) const {
  return m_textureMap.contains(imagePath);
}

void AssetTextureGroup::cleanup(std::int64_t textureTimeout) {
  if (m_reloadTracker->pullTriggered()) {
    m_textureMap.clear();
    m_textureDeduplicationMap.clear();

  } else {
    std::int64_t time = Time::monotonicMilliseconds();
    auto interval = std::clamp<std::int64_t>(textureTimeout / 10, 0, 1000);
    if (time - m_lastCleanup < interval)
      return;
    m_lastCleanup = time;

    List<std::reference_wrapper<Texture const>> liveTextures;
    filter(m_textureMap, [&](auto const& pair) {
        if (time - pair.second.second < textureTimeout) {
          liveTextures.append(std::cref(*pair.second.first));
          return true;
        }
        return false;
      });

    auto textureLess = [](auto const& lhs, auto const& rhs) {
      return std::less<Texture const*>{}(std::addressof(lhs.get()), std::addressof(rhs.get()));
    };
    std::sort(liveTextures.begin(), liveTextures.end(), textureLess);

    eraseWhere(m_textureDeduplicationMap, [&](auto const& p) {
        return !std::binary_search(liveTextures.begin(), liveTextures.end(), std::cref(*p.second), textureLess);
      });
  }
}

std::expected<TexturePtr, StarException> AssetTextureGroup::loadTexture(AssetPath const& imagePath, bool tryTexture) {
  if (auto p = m_textureMap.maybeRef(imagePath)) {
    p->get().second = Time::monotonicMilliseconds();
    return p->get().first;
  }

  auto assetsResult = m_root.assets();
  if (!assetsResult)
    return std::unexpected(std::move(assetsResult.error()));
  auto assets = *assetsResult;
  auto imageResult = tryTexture ? assets->tryImage(imagePath) : assets->image(imagePath);
  if (!imageResult)
    return std::unexpected(std::move(imageResult.error()));
  auto image = std::move(*imageResult);
  if (!image)
    return TexturePtr{};

  // Assets will return the same image ptr if two different asset paths point
  // to the same underlying cached image.  We should not make duplicate entries
  // in the texture group for these, so we keep track of the image pointers
  // returned to deduplicate them.
  if (auto existingTexture = m_textureDeduplicationMap.value(image)) {
    auto inserted = m_textureMap.add(imagePath, {existingTexture, Time::monotonicMilliseconds()});
    if (!inserted)
      return std::unexpected(std::move(inserted.error()));
    return existingTexture;
  } else {
    auto texture = m_textureGroup->create(*image);
    if (!texture)
      return std::unexpected(std::move(texture.error()));
    auto inserted = m_textureMap.add(imagePath, {*texture, Time::monotonicMilliseconds()});
    if (!inserted)
      return std::unexpected(std::move(inserted.error()));
    auto deduplicated = m_textureDeduplicationMap.add(image, *texture);
    if (!deduplicated)
      return std::unexpected(std::move(deduplicated.error()));
    return std::move(*texture);
  }
}

}
