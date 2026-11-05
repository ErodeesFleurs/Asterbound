module;
import std;
import star.byte_array;

import star.string;
import star.format;
import star.json;
import star.io_device;
import star.type_identity;

namespace Star {

struct AssetSourceExceptionTag {
  static constexpr std::string_view name() { return "AssetSourceException"; }
};
using AssetSourceException = StarError<AssetSourceExceptionTag, StarException>;

// An asset source could be a directory on a filesystem, where assets are
// pulled directly from files, or a single pak-like file containing all assets,
// where assets are pulled from the correct region of the pak-like file.
class AssetSource {
public:
  virtual ~AssetSource() = default;

  virtual void const* queryType(TypeKey key) const noexcept {
    return queryTypeOf<AssetSource>(this, key);
  }

  virtual std::string_view dynamicTypeName() const noexcept {
    return Star::typeName<AssetSource>();
  }

  // An asset source can have arbitrary metadata attached.
  virtual JsonObject metadata() const = 0;

  // Should return all the available assets in this source
  virtual StringList assetPaths() const = 0;

  // Exact, case-sensitive membership in this source's enumerated asset index.
  // Does not probe the filesystem or copy the asset path collection.
  virtual bool contains(String const& path) const = 0;

  // Open the given path in this source and return an IODevicePtr to it.
  [[nodiscard]] virtual std::expected<std::shared_ptr<IODevice>, StarException> open(String const& path) = 0;

  // Read the entirety of the given path into a buffer.
  [[nodiscard]] virtual std::expected<ByteArray, StarException> read(String const& path) = 0;
};

}

export module star.asset_source;

export namespace Star {
  using ::Star::AssetSourceExceptionTag;
  using ::Star::AssetSourceException;
  using ::Star::AssetSource;
}
