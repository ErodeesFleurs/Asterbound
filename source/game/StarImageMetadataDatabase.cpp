import star.format;
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
import star.file;
import star.image;
import star.logging;
import star.string;
import star.byte_array;
import star.encode;
import star.vector;
import star.game_types;
import star.bi_map;
import star.io_device;
import star.ref_ptr;
import star.listener;
import std;
import star.version;

import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

import star.image_metadata_database;

import star.math_common;
namespace Star {

ImageMetadataDatabase::ImageMetadataDatabase(Root& root)
  : m_root(root), m_sizeCache(root.presentationRandom()), m_spacesCache(root.presentationRandom()), m_regionCache(root.presentationRandom()) {
  std::lock_guard locker(m_mutex);
  std::chrono::milliseconds timeSmear(2000);
  std::chrono::milliseconds timeToLive(60000);
  m_sizeCache.setTimeSmear(timeSmear);
  m_spacesCache.setTimeSmear(timeSmear);
  m_regionCache.setTimeSmear(timeSmear);
  m_sizeCache.setTimeToLive(timeToLive);
  m_spacesCache.setTimeToLive(timeToLive);
  m_regionCache.setTimeToLive(timeToLive);
}

std::expected<Vec2U, StarException> ImageMetadataDatabase::imageSize(AssetPath const& path) const {
  std::unique_lock locker(m_mutex);
  auto cached = m_sizeCache.maybeRef(path);
  if (!cached)
    return std::unexpected(std::move(cached.error()));
  if (*cached)
    return (*cached)->get();
  locker.unlock();
  auto size = calculateImageSize(path);
  if (!size)
    return std::unexpected(std::move(size.error()));
  locker.lock();
  auto stored = m_sizeCache.set(path, *size);
  if (!stored)
    return std::unexpected(std::move(stored.error()));
  return *size;
}

std::expected<List<Vec2I>, StarException> ImageMetadataDatabase::imageSpaces(AssetPath const& path, Vec2F position, float fillLimit, bool flip) const {
  SpacesEntry key = std::make_tuple(path, Vec2I::round(position), fillLimit, flip);

  std::unique_lock locker(m_mutex);
  auto cached = m_spacesCache.maybeRef(key);
  if (!cached)
    return std::unexpected(std::move(cached.error()));
  if (*cached)
    return (*cached)->get();

  auto filteredPath = filterProcessing(path);
  SpacesEntry filteredKey = std::make_tuple(filteredPath, Vec2I::round(position), fillLimit, flip);

  auto filteredCached = m_spacesCache.maybeRef(filteredKey);
  if (!filteredCached)
    return std::unexpected(std::move(filteredCached.error()));
  if (*filteredCached) {
    auto stored = m_spacesCache.set(key, (*filteredCached)->get());
    if (!stored)
      return std::unexpected(std::move(stored.error()));
    return (*filteredCached)->get();
  }

  locker.unlock();

  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto image = (*assets)->image(filteredPath);
  if (!image)
    return std::unexpected(std::move(image.error()));
  int imageWidth = (*image)->width();
  int imageHeight = (*image)->height();

  Vec2I min((position / TilePixels).floor());
  Vec2I max(((Vec2F(imageWidth, imageHeight) + position) / TilePixels).ceil());

  List<Vec2I> spaces;

  for (int yspace = min[1]; yspace < max[1]; ++yspace) {
    for (int xspace = min[0]; xspace < max[0]; ++xspace) {
      float fillRatio = 0.0f;

      for (int y = 0; y < (int)TilePixels; ++y) {
        int ypixel = std::round(yspace * (int)TilePixels + y - position[1]);
        if (ypixel < 0 || ypixel >= imageHeight)
          continue;

        for (int x = 0; x < (int)TilePixels; ++x) {
          int xpixel = std::round(xspace * (int)TilePixels + x - position[0]);
          if (flip)
            xpixel = imageWidth - 1 - xpixel;

          if (xpixel < 0 || xpixel >= imageWidth)
            continue;

          auto pixel = (*image)->get(xpixel, ypixel);
          if (!pixel)
            return std::unexpected(std::move(pixel.error()));
          if ((*pixel)[3] > 0)
            fillRatio += 1.0f / square(TilePixels);
        }
      }

      if (fillRatio >= fillLimit)
        spaces.append(Vec2I(xspace, yspace));
    }
  }

  locker.lock();
  auto stored = m_spacesCache.set(key, spaces);
  if (!stored)
    return std::unexpected(std::move(stored.error()));
  auto filteredStored = m_spacesCache.set(filteredKey, spaces);
  if (!filteredStored)
    return std::unexpected(std::move(filteredStored.error()));

  return spaces;
}

std::expected<RectU, StarException> ImageMetadataDatabase::nonEmptyRegion(AssetPath const& path) const {
  std::unique_lock locker(m_mutex);
  
  auto cached = m_regionCache.maybeRef(path);
  if (!cached)
    return std::unexpected(std::move(cached.error()));
  if (*cached)
    return (*cached)->get();

  auto filteredPath = filterProcessing(path);
  auto filteredCached = m_regionCache.maybeRef(filteredPath);
  if (!filteredCached)
    return std::unexpected(std::move(filteredCached.error()));
  if (*filteredCached) {
    auto stored = m_regionCache.set(path, (*filteredCached)->get());
    if (!stored)
      return std::unexpected(std::move(stored.error()));
    return (*filteredCached)->get();
  }

  locker.unlock();
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto image = (*assets)->image(filteredPath);
  if (!image)
    return std::unexpected(std::move(image.error()));
  RectU region = RectU::null();
  (*image)->forEachPixel([&region](unsigned x, unsigned y, Vec4B const& pixel) {
    if (pixel[3] > 0)
      region.combine(RectU::withSize({x, y}, {1, 1}));
  });

  locker.lock();
  auto stored = m_regionCache.set(path, region);
  if (!stored)
    return std::unexpected(std::move(stored.error()));
  auto filteredStored = m_regionCache.set(filteredPath, region);
  if (!filteredStored)
    return std::unexpected(std::move(filteredStored.error()));

  return region;
}

void ImageMetadataDatabase::cleanup() const {
  std::lock_guard locker(m_mutex);

  m_sizeCache.cleanup();
  m_spacesCache.cleanup();
  m_regionCache.cleanup();
}

AssetPath ImageMetadataDatabase::filterProcessing(AssetPath const& path) {
  AssetPath newPath = { path.basePath, path.subPath, {} };

  String filtered;
  for (auto& directives : path.directives.list())
    directives.loadOperations();
  path.directives.forEach([&](auto const& entry, Directives const& directives) {
    ImageOperation const& operation = entry.operation;
    if (!(std::holds_alternative<HueShiftImageOperation>(operation)           ||
          std::holds_alternative<SaturationShiftImageOperation>(operation)    ||
          std::holds_alternative<BrightnessMultiplyImageOperation>(operation) ||
          std::holds_alternative<FadeToColorImageOperation>(operation)        ||
          std::holds_alternative<ScanLinesImageOperation>(operation)          ||
          std::holds_alternative<SetColorImageOperation>(operation))) {
      filtered += "?";
      filtered += entry.string(*directives.m_shared);
    }
    });

  newPath.directives = std::move(filtered);
  return newPath;
}

std::expected<Vec2U, StarException> ImageMetadataDatabase::calculateImageSize(AssetPath const& path) const {
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto fallback = [&assets, &path]() -> std::expected<Vec2U, StarException> {
    auto image = (*assets)->image(path);
    if (!image)
      return std::unexpected(std::move(image.error()));
    return (*image)->size();
  };
  auto exists = (*assets)->assetExists(path.basePath);
  if (!exists)
    return std::unexpected(std::move(exists.error()));
  if (!*exists)
    return fallback();
  Vec2U imageSize;
  if (path.subPath) {
    auto frames = (*assets)->imageFrames(path.basePath);
    if (!frames)
      return std::unexpected(std::move(frames.error()));
    if (!*frames)
      return fallback();
    auto rect = (*frames)->getRect(*path.subPath);
    if (!rect)
      return std::unexpected(std::move(rect.error()));
    if (*rect)
      imageSize = (**rect).size();
    else
      return fallback();
  } else {
    std::unique_lock locker(m_mutex);
    auto size = m_sizeCache.maybeRef(path.basePath);
    if (!size)
      return std::unexpected(std::move(size.error()));
    if (*size) {
      imageSize = (*size)->get();
    } else {
      locker.unlock();
      auto file = (*assets)->openFile(path.basePath);
      if (!file)
        return std::unexpected(std::move(file.error()));
      auto png = Image::isPng(*file);
      if (!png)
        return std::unexpected(std::move(png.error()));
      if (*png) {
        auto metadata = Image::readPngMetadata(m_root.logger(), *file);
        if (!metadata)
          return std::unexpected(std::move(metadata.error()));
        imageSize = std::get<0>(*metadata);
      } else {
        auto size = fallback();
        if (!size)
          return std::unexpected(std::move(size.error()));
        imageSize = *size;
      }
      locker.lock();
      auto stored = m_sizeCache.set(path.basePath, imageSize);
      if (!stored)
        return std::unexpected(std::move(stored.error()));
    }
  }

  struct OperationSizeAdjust {
    Vec2U& imageSize;
    bool hasError;

    OperationSizeAdjust(Vec2U& size) : imageSize(size), hasError(false) {};

    void operator()(NullImageOperation const&) {}

    void operator()(ErrorImageOperation const&) {}

    void operator()(HueShiftImageOperation const&) {}

    void operator()(SaturationShiftImageOperation const&) {}

    void operator()(BrightnessMultiplyImageOperation const&) {}

    void operator()(FadeToColorImageOperation const&) {}

    void operator()(ScanLinesImageOperation const&) {}

    void operator()(SetColorImageOperation const&) {}

    void operator()(ColorReplaceImageOperation const&) {}

    void operator()(AlphaMaskImageOperation const&) {}

    void operator()(BlendImageOperation const&) {}

    void operator()(MultiplyImageOperation const&) {}

    void operator()(BorderImageOperation const& bio) {
      imageSize += Vec2U::filled(bio.pixels * 2);
    }

    void operator()(ScaleImageOperation const& sio) {
      auto size = vmult(Vec2F(imageSize), sio.scale);
      if (!std::isfinite(size[0]) || !std::isfinite(size[1]) || size[0] <= 0 || size[1] <= 0
          || static_cast<double>(size[0]) > std::numeric_limits<unsigned>::max()
          || static_cast<double>(size[1]) > std::numeric_limits<unsigned>::max()) {
        hasError = true;
      } else {
        imageSize = Vec2U::round(size);
      }
    }

    void operator()(CropImageOperation const& cio) {
      if (cio.subset.isEmpty() ||
          cio.subset.xMin() < 0 ||
          cio.subset.yMin() < 0 ||
          (unsigned)cio.subset.xMax() > imageSize[0] ||
          (unsigned)cio.subset.yMax() > imageSize[1]) {
        hasError = true;
      } else {
        imageSize = Vec2U(cio.subset.size());
      }
    }

    void operator()(FlipImageOperation const&) {}
  };

  OperationSizeAdjust osa(imageSize);

  for (auto& directives : path.directives.list())
    directives.loadOperations();

  bool complete = path.directives.forEachAbortable([&](auto const& entry, Directives const&) -> bool {
    std::visit(osa, entry.operation);
    return !osa.hasError;
  });

  if (!complete)
    return fallback();

  return imageSize;
}

}
