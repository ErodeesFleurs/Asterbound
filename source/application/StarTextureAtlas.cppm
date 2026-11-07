module;

import std;
import star.memory;
import star.rect;
import star.format;
import star.set;
import star.list;
import star.image;
import star.casting;
import star.type_identity;

import star.vector;
import star.math_common;
import star.algorithm;
namespace Star {

struct TextureAtlasExceptionTag {
  static constexpr std::string_view name() { return "TextureAtlasException"; }
};
using TextureAtlasException = StarError<TextureAtlasExceptionTag, StarException>;

// Implements a set of "texture atlases" or, sets of smaller textures grouped
// as a larger texture.
template <typename AtlasTextureHandle>
class TextureAtlasSet {
public:
  struct Texture {
    virtual void const* queryType(TypeKey key) const noexcept {
      return queryTypeOf<Texture>(this, key);
    }
    virtual std::string_view dynamicTypeName() const noexcept {
      return Star::typeName<Texture>();
    }

    virtual Vec2U imageSize() const = 0;

    virtual AtlasTextureHandle const& atlasTexture() const = 0;
    virtual RectU atlasTextureCoordinates() const = 0;

    // A locked texture will never be moved during compression, so its
    // atlasTexture and textureCoordinates will not change.
    virtual void setLocked(bool locked) = 0;

    // Returns true if this texture has been freed or the parent
    // TextureAtlasSet has been destructed.
    virtual bool expired() const = 0;
  };

  using TextureHandle = std::shared_ptr<Texture>;

  TextureAtlasSet(unsigned cellSize, unsigned atlasNumCells);

  virtual ~TextureAtlasSet() {
    for (auto const& texture : m_textures)
      texture->atlasPlacement.atlas.reset();
  }

  // The constant square size of all atlas textures
  Vec2U atlasTextureSize() const;

  // Removes all existing textures and destroys all texture atlases.
  void reset();

  // Adds texture to some TextureAtlas.  Texture must fit in a single atlas
  // texture, otherwise an error is returned. Returns an owning handle to the
  // texture. If borderPixels is true, then fills a 1px border around the
  // given image in the atlas with the nearest color value, to prevent
  // bleeding.
  [[nodiscard]] std::expected<TextureHandle, StarException> addTexture(Image const& image, bool borderPixels = true);

  // Removes the given texture from the TextureAtlasSet and expires its handle.
  [[nodiscard]] std::expected<void, StarException> freeTexture(TextureHandle const& texture);

  unsigned totalAtlases() const;
  unsigned totalTextures() const;
  float averageFillLevel() const;

  // Takes images from sparsely filled atlases and moves them to less sparsely
  // filled atlases in an effort to free up room.  This method tages the atlas
  // with the lowest fill level and picks a texture from it, removes it, and
  // re-adds it to the AtlasSet.  It does this up to textureCount textures,
  // until it finds a texture where re-adding it to the texture atlas simply
  // moves the texture into the same atlas, at which point it stops.
  [[nodiscard]] std::expected<void, StarException> compressionPass(std::size_t textureCount = NPos);

  // The number of atlases that the AtlasSet will attempt to fit a texture in
  // before giving up and creating a new atlas.  Tries in order of least full
  // to most full.  Defaults to 3.
  unsigned textureFitTries() const;
  void setTextureFitTries(unsigned textureFitTries);

protected:
  [[nodiscard]] virtual std::expected<AtlasTextureHandle, StarException> createAtlasTexture(Vec2U const& size, PixelFormat pixelFormat) = 0;
  virtual void destroyAtlasTexture(AtlasTextureHandle const& atlasTexture) = 0;
  [[nodiscard]] virtual std::expected<void, StarException> copyAtlasPixels(AtlasTextureHandle const& atlasTexture, Vec2U const& bottomLeft, Image const& image) = 0;

private:
  struct TextureAtlas {
    AtlasTextureHandle atlasTexture;
    std::unique_ptr<bool[]> usedCells;
    unsigned usedCellCount;
  };

  struct AtlasPlacement {
    std::optional<std::reference_wrapper<TextureAtlas>> atlas;
    bool borderPixels = false;
    RectU occupiedCells;
    RectU textureCoords;
  };

  struct TextureEntry : Texture {
    virtual ~TextureEntry() = default;

    void const* queryType(TypeKey key) const noexcept override {
      return queryTypeOf<TextureEntry, Texture>(this, key);
    }
    std::string_view dynamicTypeName() const noexcept override {
      return Star::typeName<TextureEntry>();
    }

    Vec2U imageSize() const override;

    AtlasTextureHandle const& atlasTexture() const override;
    RectU atlasTextureCoordinates() const override;

    // A locked texture will never be moved during compression, so its
    // atlasTexture and textureCoordinates will not change.
    void setLocked(bool locked) override;

    bool expired() const override;

    Image textureImage;
    AtlasPlacement atlasPlacement;
    bool placementLocked = false;
  };

  void setAtlasRegionUsed(TextureAtlas& textureAtlas, RectU const& region, bool used) const;

  [[nodiscard]] std::expected<std::optional<AtlasPlacement>, StarException> addTextureToAtlas(TextureAtlas& atlas, Image const& image, bool borderPixels);
  void sortAtlases();

  unsigned m_atlasCellSize;
  unsigned m_atlasNumCells;
  unsigned m_textureFitTries = 3;

  List<std::unique_ptr<TextureAtlas>> m_atlases;
  HashSet<std::shared_ptr<TextureEntry>> m_textures;
};

template <typename AtlasTextureHandle>
TextureAtlasSet<AtlasTextureHandle>::TextureAtlasSet(unsigned cellSize, unsigned atlasNumCells)
    : m_atlasCellSize(cellSize), m_atlasNumCells(atlasNumCells) {}

template <typename AtlasTextureHandle>
Vec2U TextureAtlasSet<AtlasTextureHandle>::atlasTextureSize() const {
  return Vec2U::filled(m_atlasCellSize * m_atlasNumCells);
}

template <typename AtlasTextureHandle>
void TextureAtlasSet<AtlasTextureHandle>::reset() {
  for (auto const& texture : m_textures)
    texture->atlasPlacement.atlas.reset();

  for (auto const& atlas : m_atlases)
    destroyAtlasTexture(atlas->atlasTexture);

  m_atlases.clear();
  m_textures.clear();
}

template <typename AtlasTextureHandle>
[[nodiscard]] auto TextureAtlasSet<AtlasTextureHandle>::addTexture(Image const& image, bool borderPixels) -> std::expected<TextureHandle, StarException> {
  if (image.empty())
    return std::unexpected(TextureAtlasException("Empty image given in TextureAtlasSet::addTexture"));

  Image finalImage;
  if (borderPixels) {
    Vec2U imageSize = image.size();
    Vec2U finalImageSize = imageSize + Vec2U(2, 2);
    finalImage = Image(finalImageSize, PixelFormat::RGBA32);

    // Fill 1px border on all sides of the image
    for (unsigned y = 0; y < finalImageSize[1]; ++y) {
      for (unsigned x = 0; x < finalImageSize[0]; ++x) {
        unsigned xind = std::clamp<unsigned>(x, 1, imageSize[0]) - 1;
        unsigned yind = std::clamp<unsigned>(y, 1, imageSize[1]) - 1;
        auto pixel = image.getrgb(xind, yind);
        if (!pixel)
          return std::unexpected(std::move(pixel.error()));
        finalImage.set32(x, y, *pixel);
      }
    }
  } else {
    finalImage = image;
  }

  auto tryAtlas = [&](TextureAtlas& atlas) -> std::expected<TextureHandle, StarException> {
    auto placement = addTextureToAtlas(atlas, finalImage, borderPixels);
    if (!placement)
      return std::unexpected(std::move(placement.error()));
    if (!*placement)
      return TextureHandle{};

    auto textureEntry = std::make_shared<TextureEntry>();
    textureEntry->textureImage = std::move(finalImage);
    textureEntry->atlasPlacement = **placement;

    m_textures.add(textureEntry);
    sortAtlases();
    return textureEntry;
  };

  // Try the first 'm_textureFitTries' atlases to see if we can fit a given
  // texture in an existing atlas.  Do this from the most full to the least
  // full atlas to maximize compression.
  std::size_t startAtlas = m_atlases.size() - std::min<std::size_t>(m_atlases.size(), m_textureFitTries);
  for (std::size_t i = startAtlas; i < m_atlases.size(); ++i) {
    auto texture = tryAtlas(*m_atlases[i]);
    if (!texture)
      return std::unexpected(std::move(texture.error()));
    if (*texture)
      return std::move(*texture);
  }

  // If we have not found an existing atlas to put the texture, need to create
  // a new atlas
  auto atlasTexture = createAtlasTexture(atlasTextureSize(), PixelFormat::RGBA32);
  if (!atlasTexture)
    return std::unexpected(std::move(atlasTexture.error()));
  m_atlases.append(std::make_unique<TextureAtlas>(TextureAtlas{
      std::move(*atlasTexture),
      std::unique_ptr<bool[]>(new bool[m_atlasNumCells * m_atlasNumCells]()), 0
    }));

  auto texture = tryAtlas(*m_atlases[m_atlases.size() - 1]);
  if (texture && *texture)
    return std::move(*texture);

  // Failed placement never commits occupancy; remove the newly allocated atlas.
  destroyAtlasTexture(m_atlases[m_atlases.size() - 1]->atlasTexture);
  m_atlases.pop_back();
  if (!texture)
    return std::unexpected(std::move(texture.error()));
  return std::unexpected(TextureAtlasException("Could not add texture to new atlas in TextureAtlasSet::addTexture, too large"));
}

template <typename AtlasTextureHandle>
[[nodiscard]] std::expected<void, StarException> TextureAtlasSet<AtlasTextureHandle>::freeTexture(TextureHandle const& texture) {
  auto textureEntry = as<TextureEntry>(texture);
  if (!textureEntry || !m_textures.contains(textureEntry))
    return std::unexpected(TextureAtlasException("Texture does not belong to this atlas set or has already expired"));

  setAtlasRegionUsed(textureEntry->atlasPlacement.atlas->get(), textureEntry->atlasPlacement.occupiedCells, false);
  sortAtlases();

  textureEntry->atlasPlacement.atlas.reset();
  m_textures.remove(textureEntry);
  return {};
}

template <typename AtlasTextureHandle>
unsigned TextureAtlasSet<AtlasTextureHandle>::totalAtlases() const {
  return m_atlases.size();
}

template <typename AtlasTextureHandle>
unsigned TextureAtlasSet<AtlasTextureHandle>::totalTextures() const {
  return m_textures.size();
}

template <typename AtlasTextureHandle>
float TextureAtlasSet<AtlasTextureHandle>::averageFillLevel() const {
  if (m_atlases.empty())
    return 0.0f;

  float atlasFillLevelSum = 0.0f;
  for (auto const& atlas : m_atlases)
    atlasFillLevelSum += atlas->usedCellCount / (float)square(m_atlasNumCells);
  return atlasFillLevelSum / m_atlases.size();
}

template <typename AtlasTextureHandle>
[[nodiscard]] std::expected<void, StarException> TextureAtlasSet<AtlasTextureHandle>::compressionPass(std::size_t textureCount) {
  while (m_atlases.size() > 1 && textureCount > 0) {
    // Find the least full atlas, If it is empty, remove it and start at the
    // next atlas.
    auto const& smallestAtlas = m_atlases[m_atlases.size() - 1];
    if (smallestAtlas->usedCellCount == 0) {
      destroyAtlasTexture(smallestAtlas->atlasTexture);
      m_atlases.pop_back();
      continue;
    }

    // Loop over the currently loaded textures to find the smallest texture in
    // the smallest atlas that is not locked.
    std::optional<std::reference_wrapper<TextureEntry>> smallestTexture;
    for (auto const& texture : m_textures) {
      if (std::addressof(texture->atlasPlacement.atlas->get()) == std::addressof(*smallestAtlas)) {
        if (!texture->placementLocked) {
          if (!smallestTexture || texture->atlasPlacement.occupiedCells.volume() < smallestTexture->get().atlasPlacement.occupiedCells.volume())
            smallestTexture = std::ref(*texture);
        }
      }
    }

    // If we were not able to find a smallest texture because the texture is
    // locked, then simply stop.  TODO: This could be done better, this will
    // prevent compressing textures that are not from the smallest atlas if the
    // smallest atlas has only locked textures.
    if (!smallestTexture)
      break;

    // Try to add the texture to any atlas that isn't the last (most empty) one
    std::size_t startAtlas = m_atlases.size() - 1 - std::min<std::size_t>(m_atlases.size() - 1, m_textureFitTries);
    for (std::size_t i = startAtlas; i < m_atlases.size() - 1; ++i) {
      auto& entry = smallestTexture->get();
      auto placement = addTextureToAtlas(*m_atlases[i], entry.textureImage, entry.atlasPlacement.borderPixels);
      if (!placement)
        return std::unexpected(std::move(placement.error()));
      if (*placement) {
        setAtlasRegionUsed(entry.atlasPlacement.atlas->get(), entry.atlasPlacement.occupiedCells, false);
        entry.atlasPlacement = **placement;
        smallestTexture.reset();
        sortAtlases();
        break;
      }
    }

    // If we have not managed to move the smallest texture into any other
    // atlas, assume the atlas set is compressed enough and quit.
    if (smallestTexture)
      break;

    --textureCount;
  }
  return {};
}

template <typename AtlasTextureHandle>
unsigned TextureAtlasSet<AtlasTextureHandle>::textureFitTries() const {
  return m_textureFitTries;
}

template <typename AtlasTextureHandle>
void TextureAtlasSet<AtlasTextureHandle>::setTextureFitTries(unsigned textureFitTries) {
  m_textureFitTries = textureFitTries;
}

template <typename AtlasTextureHandle>
Vec2U TextureAtlasSet<AtlasTextureHandle>::TextureEntry::imageSize() const {
  if (atlasPlacement.borderPixels)
    return textureImage.size() - Vec2U(2, 2);
  else
    return textureImage.size();
}

template <typename AtlasTextureHandle>
AtlasTextureHandle const& TextureAtlasSet<AtlasTextureHandle>::TextureEntry::atlasTexture() const {
  return atlasPlacement.atlas->get().atlasTexture;
}

template <typename AtlasTextureHandle>
RectU TextureAtlasSet<AtlasTextureHandle>::TextureEntry::atlasTextureCoordinates() const {
  return atlasPlacement.textureCoords;
}

template <typename AtlasTextureHandle>
void TextureAtlasSet<AtlasTextureHandle>::TextureEntry::setLocked(bool locked) {
  placementLocked = locked;
}

template <typename AtlasTextureHandle>
bool TextureAtlasSet<AtlasTextureHandle>::TextureEntry::expired() const {
  return !atlasPlacement.atlas;
}

template <typename AtlasTextureHandle>
void TextureAtlasSet<AtlasTextureHandle>::setAtlasRegionUsed(TextureAtlas& textureAtlas, RectU const& region, bool used) const {
  for (unsigned y = region.yMin(); y < region.yMax(); ++y) {
    for (unsigned x = region.xMin(); x < region.xMax(); ++x) {
      auto& val = textureAtlas.usedCells[y * m_atlasNumCells + x];
      bool oldVal = val;
      val = used;
      if (oldVal && !val) {
        if constexpr (Star::DebugEnabled) {
          Star::starAssertFailed((textureAtlas.usedCellCount != 0));
        }
        textureAtlas.usedCellCount -= 1;
      } else if (!oldVal && used) {
        textureAtlas.usedCellCount += 1;
        if constexpr (Star::DebugEnabled) {
          Star::starAssertFailed((textureAtlas.usedCellCount <= square(m_atlasNumCells)));
        }
      }
    }
  }
}

template <typename AtlasTextureHandle>
void TextureAtlasSet<AtlasTextureHandle>::sortAtlases() {
  std::sort(m_atlases.begin(), m_atlases.end(), [](auto const& a1, auto const& a2) {
      return a1->usedCellCount > a2->usedCellCount;
    });
}

template <typename AtlasTextureHandle>
[[nodiscard]] auto TextureAtlasSet<AtlasTextureHandle>::addTextureToAtlas(TextureAtlas& atlas, Image const& image, bool borderPixels) -> std::expected<std::optional<AtlasPlacement>, StarException> {
  bool found = false;
  // Minimum cell indexes where this texture fits in this atlas.
  unsigned fitCellX = 0;
  unsigned fitCellY = 0;

  Vec2U imageSize = image.size();

  // Number of cells this image will take.
  std::size_t numCellsX = (imageSize[0] + m_atlasCellSize - 1) / m_atlasCellSize;
  std::size_t numCellsY = (imageSize[1] + m_atlasCellSize - 1) / m_atlasCellSize;

  if (numCellsX > m_atlasNumCells || numCellsY > m_atlasNumCells)
    return std::nullopt;

  for (std::size_t cellY = 0; cellY <= m_atlasNumCells - numCellsY; ++cellY) {
    for (std::size_t cellX = 0; cellX <= m_atlasNumCells - numCellsX; ++cellX) {
      // Check this box of numCellsX x numCellsY for fit.
      found = true;
      std::size_t fx;
      std::size_t fy;
      for (fy = cellY; fy < cellY + numCellsY; ++fy) {
        for (fx = cellX; fx < cellX + numCellsX; ++fx) {
          if (atlas.usedCells[fy * m_atlasNumCells + fx]) {
            found = false;
            break;
          }
        }
        if (!found)
          break;
      }
      if (!found) {
        // If it does not fit, then we can skip to the block past the first
        // horizontal used block;
        cellX = fx;
      } else {
        fitCellX = cellX;
        fitCellY = cellY;
        break;
      }
    }
    if (found)
      break;
  }

  if (!found)
    return std::nullopt;

  auto copied = copyAtlasPixels(atlas.atlasTexture, Vec2U(fitCellX * m_atlasCellSize, fitCellY * m_atlasCellSize), image);
  if (!copied)
    return std::unexpected(std::move(copied.error()));
  setAtlasRegionUsed(atlas, RectU::withSize({fitCellX, fitCellY}, {(unsigned)numCellsX, (unsigned)numCellsY}), true);

  AtlasPlacement atlasPlacement{std::ref(atlas), borderPixels, {}, {}};
  atlasPlacement.occupiedCells = RectU::withSize(Vec2U(fitCellX, fitCellY), Vec2U(numCellsX, numCellsY));
  if (borderPixels)
    atlasPlacement.textureCoords = RectU::withSize(Vec2U(fitCellX * m_atlasCellSize + 1, fitCellY * m_atlasCellSize + 1), imageSize - Vec2U(2, 2));
  else
    atlasPlacement.textureCoords = RectU::withSize(Vec2U(fitCellX * m_atlasCellSize, fitCellY * m_atlasCellSize), imageSize);
  return atlasPlacement;
}

}

export module star.texture_atlas;

export namespace Star {
  using ::Star::TextureAtlasExceptionTag;
  using ::Star::TextureAtlasException;
  using ::Star::TextureAtlasSet;
}
