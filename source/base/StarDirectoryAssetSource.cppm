module;
import std;
import star.format;
import star.byte_array;

import star.list;
import star.json;
import star.io_device;
import star.asset_source;
import star.string;
import star.type_identity;

namespace Star {

class Logger;

class DirectoryAssetSource : public AssetSource {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<DirectoryAssetSource, AssetSource>(this, key);
  }

  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<DirectoryAssetSource>();
  }

  // Any file that forms an asset path that matches any of the patterns in
  // 'ignorePatterns' is ignored.
  [[nodiscard]] static std::expected<std::shared_ptr<DirectoryAssetSource>, StarException> create(
      Logger& logger, String const& baseDirectory, StringList const& ignorePatterns = {});

  JsonObject metadata() const override;
  StringList assetPaths() const override;
  bool contains(String const& path) const override;

  [[nodiscard]] std::expected<std::shared_ptr<IODevice>, StarException> open(String const& path) override;
  [[nodiscard]] std::expected<ByteArray, StarException> read(String const& path) override;

  // Converts an asset path to the path on the filesystem
  [[nodiscard]] std::expected<String, StarException> toFilesystem(String const& path) const;

  // Update metadata file or add a new one.
  [[nodiscard]] std::expected<void, StarException> setMetadata(JsonObject metadata);

private:
  DirectoryAssetSource(Logger& logger, String const& baseDirectory, StringList const& ignorePatterns);
  // 'ancestorDirectories' holds the filesystem path of every directory on the
  // current recursion chain, so that a symlink leading back into one of them is
  // skipped instead of being followed until the operating system errors out.
  [[nodiscard]] std::expected<void, StarException> scanAll(String const& assetDirectory, StringList& output, List<String> const& ancestorDirectories) const;

  Logger& m_logger;
  String m_baseDirectory;
  List<String> m_ignorePatterns;
  std::optional<String> m_metadataFile;
  JsonObject m_metadata;
  StringList m_assetPaths;
};

}

export module star.directory_asset_source;

export namespace Star {
  using ::Star::DirectoryAssetSource;
}
