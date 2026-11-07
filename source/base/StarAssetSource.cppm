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

  // Fresh checked length without constructing a payload reader.
  [[nodiscard]] virtual std::expected<StreamOffset, StarException> file_size(String const& path) = 0;

  // Results correspond exactly to input order, including duplicates and errors.
  // The default checks each entry freshly via file_size. Packed sources capture
  // one checked backing extent for this call, not immutable file contents; no
  // snapshot or result is cached across calls. Empty input performs no I/O.
  [[nodiscard]] virtual std::vector<std::expected<StreamOffset, StarException>>
  file_sizes(std::span<std::reference_wrapper<String const> const> paths) {
    std::vector<std::expected<StreamOffset, StarException>> sizes;
    sizes.reserve(paths.size());
    for (auto path : paths)
      sizes.push_back(file_size(path.get()));
    return sizes;
  }

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
