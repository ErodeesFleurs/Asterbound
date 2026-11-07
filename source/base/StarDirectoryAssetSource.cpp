import std;
import star.list;
import star.format;
import star.json;
import star.io_device;
import star.asset_source;
import star.string;
import star.directory_asset_source;
import star.file;
import star.json_extra;
import star.logging;
import star.byte_array;

namespace Star {
namespace {
  bool resolvesToAncestor(String const& directory, List<String> const& ancestors) {
    std::error_code ec;
    std::filesystem::path directoryPath(directory.utf8());
    for (auto const& ancestor : ancestors) {
      if (std::filesystem::equivalent(directoryPath, std::filesystem::path(ancestor.utf8()), ec) && !ec)
        return true;
    }
    return false;
  }
}

DirectoryAssetSource::DirectoryAssetSource(Logger& logger, String const& baseDirectory, StringList const& ignorePatterns)
  : m_logger(logger), m_baseDirectory(baseDirectory), m_ignorePatterns(ignorePatterns) {}

std::expected<std::shared_ptr<DirectoryAssetSource>, StarException> DirectoryAssetSource::create(
    Logger& logger, String const& baseDirectory, StringList const& ignorePatterns) {
  auto source = std::shared_ptr<DirectoryAssetSource>(new DirectoryAssetSource(logger, baseDirectory, ignorePatterns));
  // Load metadata from either /_metadata or /.metadata, in that order.
  for (auto fileName : std::array<std::string_view, 2>{"/_metadata", "/.metadata"}) {
    auto metadataFile = source->toFilesystem(String(fileName));
    if (!metadataFile)
      return std::unexpected(std::move(metadataFile.error()));
    auto exists = File::is_regular_file(*metadataFile);
    if (!exists)
      return std::unexpected(std::move(exists.error()));
    if (*exists) {
      auto contents = File::read_file_string(*metadataFile);
      if (!contents)
        return std::unexpected(std::move(contents.error()));
      auto parsed = Json::parseJson(*contents);
      if (!parsed)
        return std::unexpected(AssetSourceException(strf("Could not load metadata file '{}' from assets", *metadataFile), parsed.error()));
      auto metadata = parsed->toObject();
      if (!metadata)
        return std::unexpected(AssetSourceException(strf("Could not load metadata file '{}' from assets", *metadataFile), metadata.error()));
      source->m_metadataFile = String(fileName);
      source->m_metadata = std::move(*metadata);
      break;
    }
  }
  source->m_ignorePatterns.append("^/_metadata$");
  source->m_ignorePatterns.append("^/\\.metadata$");
  auto root = source->toFilesystem("/");
  if (!root)
    return std::unexpected(std::move(root.error()));
  auto scanned = source->scanAll("/", source->m_assetPaths, {*root});
  if (!scanned)
    return std::unexpected(std::move(scanned.error()));
  source->m_assetPaths.sort();
  return source;
}

JsonObject DirectoryAssetSource::metadata() const { return m_metadata; }
StringList DirectoryAssetSource::assetPaths() const { return m_assetPaths; }
bool DirectoryAssetSource::contains(String const& path) const {
  return std::binary_search(m_assetPaths.begin(), m_assetPaths.end(), path);
}

std::expected<StreamOffset, StarException> DirectoryAssetSource::file_size(String const& path) {
  if (!contains(path))
    return std::unexpected(
      AssetSourceException::format("Requested file '{}' does not exist in the directory assets", path));
  auto filesystem = toFilesystem(path);
  if (!filesystem)
    return std::unexpected(std::move(filesystem.error()));
  return File::file_size(*filesystem);
}

std::expected<std::shared_ptr<IODevice>, StarException> DirectoryAssetSource::open(String const& path) {
  auto filesystem = toFilesystem(path);
  if (!filesystem)
    return std::unexpected(std::move(filesystem.error()));
  auto file = File::open(*filesystem, IOMode::Read);
  if (!file)
    return std::unexpected(std::move(file.error()));
  return std::shared_ptr<IODevice>(std::move(*file));
}

std::expected<ByteArray, StarException> DirectoryAssetSource::read(String const& path) {
  auto device = open(path);
  if (!device)
    return std::unexpected(std::move(device.error()));
  auto size = (*device)->size();
  if (!size)
    return std::unexpected(std::move(size.error()));
  return (*device)->read_bytes(*size);
}

std::expected<String, StarException> DirectoryAssetSource::toFilesystem(String const& path) const {
  auto absolute = path.beginsWith("/");
  if (!absolute)
    return std::unexpected(std::move(absolute.error()));
  if (!*absolute)
    return std::unexpected(AssetSourceException::format("Asset path '{}' must be absolute in DirectoryAssetSource::toFilesystem", path));
  auto relative = path.substr(1);
  if (!relative)
    return std::unexpected(std::move(relative.error()));
  return File::relative_to(m_baseDirectory, File::convert_dir_separators(*relative));
}

std::expected<void, StarException> DirectoryAssetSource::setMetadata(JsonObject metadata) {
  if (metadata != m_metadata) {
    String metadataFile = m_metadataFile ? *m_metadataFile : String("/_metadata");
    auto filesystem = toFilesystem(metadataFile);
    if (!filesystem)
      return std::unexpected(std::move(filesystem.error()));
    if (metadata.empty()) {
      auto removed = File::remove(*filesystem);
      if (!removed)
        return std::unexpected(std::move(removed.error()));
    } else {
      auto printed = Json(metadata).printJson(2, true);
      if (!printed)
        return std::unexpected(std::move(printed.error()));
      auto written = File::write_file(*printed, *filesystem);
      if (!written)
        return std::unexpected(std::move(written.error()));
    }
    m_metadataFile = std::move(metadataFile);
    m_metadata = std::move(metadata);
  }
  return {};
}

std::expected<void, StarException> DirectoryAssetSource::scanAll(String const& assetDirectory, StringList& output,
    List<String> const& ancestorDirectories) const {
  auto shouldIgnore = [this](String const& assetPath) -> std::expected<bool, StarException> {
    for (auto const& pattern : m_ignorePatterns) {
      auto matches = assetPath.regexMatch(pattern, false, false);
      if (!matches)
        return std::unexpected(std::move(matches.error()));
      if (*matches)
        return true;
    }
    return false;
  };
  auto fsDirectory = toFilesystem(assetDirectory);
  if (!fsDirectory)
    return std::unexpected(std::move(fsDirectory.error()));
  auto entries = File::directory_entries(*fsDirectory);
  if (!entries)
    return std::unexpected(std::move(entries.error()));
  for (auto const& entry : *entries) {
    String assetPath = assetDirectory + entry.first;
    if (entry.second) {
      auto childDirectory = toFilesystem(assetPath);
      if (!childDirectory)
        return std::unexpected(std::move(childDirectory.error()));
      if (resolvesToAncestor(*childDirectory, ancestorDirectories)) {
        auto warned = m_logger.warn("DirectoryAssetSource: '{}' resolves to one of its own parent directories, not scanning it", assetPath);
        if (!warned)
          return std::unexpected(std::move(warned.error()));
        continue;
      }
      auto childAncestors = ancestorDirectories;
      childAncestors.append(std::move(*childDirectory));
      auto scanned = scanAll(assetPath + "/", output, childAncestors);
      if (!scanned)
        return std::unexpected(std::move(scanned.error()));
    } else {
      auto ignored = shouldIgnore(assetPath);
      if (!ignored)
        return std::unexpected(std::move(ignored.error()));
      if (!*ignored)
        output.append(std::move(assetPath));
    }
  }
  return {};
}
}
