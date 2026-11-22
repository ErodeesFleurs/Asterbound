module;
import std;
import star.format;
import star.byte_array;

import star.json;
import star.io_device;
import star.asset_source;
import star.string;

namespace Star {

class Logger;

class DirectoryAssetSource : public AssetSource {
  struct ConstructionKey { explicit ConstructionKey() = default; };

public:
  DirectoryAssetSource(ConstructionKey, String base_directory);
  DirectoryAssetSource(DirectoryAssetSource const&) = delete;
  DirectoryAssetSource& operator=(DirectoryAssetSource const&) = delete;

  // Ignore matching files, not directories. Patterns are reached in declaration order.
  [[nodiscard]] static std::expected<std::shared_ptr<DirectoryAssetSource>, StarException> create(
      Logger& logger, String base_directory, StringList const& ignore_patterns = {});

  JsonObject const& metadata() const & override;
  JsonObject const& metadata() const && = delete;
  std::size_t size() const override;
  StringList asset_paths() const override;
  // The scanned path index stays fixed throughout this source's lifetime.
  std::span<String const> path_view() const &;
  std::span<String const> path_view() const && = delete;
  bool contains(String const& path) const override;
  [[nodiscard]] std::expected<StreamOffset, StarException> file_size(String const& path) override;
  std::optional<String> missing_file_path(String const& path) const override;

  [[nodiscard]] std::expected<std::shared_ptr<IODevice>, StarException> open(String const& path) override;
  [[nodiscard]] std::expected<ByteArray, StarException> read(String const& path) override;

  // Converts an asset path to the path on the filesystem
  [[nodiscard]] std::expected<String, StarException> to_filesystem(String const& path) const;

  // Update metadata file or add a new one.
  [[nodiscard]] std::expected<void, StarException> set_metadata(JsonObject metadata);

private:
  [[nodiscard]] std::expected<void, StarException> visit_paths(PathVisitor const& visit) const override;
  [[nodiscard]] std::expected<void, StarException> scan(Logger& logger, StringList const& ignore_patterns);

  String m_base_directory;
  bool m_uses_dot_metadata = false;
  JsonObject m_metadata;
  StringList m_asset_paths;
};

}

export module star.directory_asset_source;

export namespace Star {
  using ::Star::DirectoryAssetSource;
}
