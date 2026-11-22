import std;
import star.list;
import star.format;
import star.json;
import star.io_device;
import star.asset_source;
import star.string;
import star.directory_asset_source;
import star.file;
import star.logging;
import star.byte_array;

namespace Star {
namespace {
  std::expected<std::filesystem::path, StarException> native_path(String const& path) {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
    auto native = path.to_wstring();
    if (!native)
      return std::unexpected(std::move(native.error()));
    return std::filesystem::path(std::move(*native));
#else
    return std::filesystem::path(path.utf8());
#endif
  }
}

DirectoryAssetSource::DirectoryAssetSource(ConstructionKey, String base_directory)
  : m_base_directory(std::move(base_directory)) {}

std::expected<std::shared_ptr<DirectoryAssetSource>, StarException> DirectoryAssetSource::create(
    Logger& logger, String base_directory, StringList const& ignore_patterns) {
  auto source = std::make_shared<DirectoryAssetSource>(ConstructionKey{}, std::move(base_directory));
  // Load metadata from either /_metadata or /.metadata, in that order.
  for (auto file_name : std::array<std::string_view, 2>{"/_metadata", "/.metadata"}) {
    auto metadata_file = source->to_filesystem(String(file_name));
    if (!metadata_file)
      return std::unexpected(std::move(metadata_file.error()));
    auto exists = File::is_regular_file(*metadata_file);
    if (!exists)
      return std::unexpected(std::move(exists.error()));
    if (*exists) {
      auto contents = File::read_file_string(*metadata_file);
      if (!contents)
        return std::unexpected(std::move(contents.error()));
      auto parsed = Json::parse_json(*contents);
      if (!parsed)
        return std::unexpected(AssetSourceException(Star::format("Could not load metadata file '{}' from assets", *metadata_file), parsed.error()));
      auto metadata = parsed->to_object();
      if (!metadata)
        return std::unexpected(AssetSourceException(Star::format("Could not load metadata file '{}' from assets", *metadata_file), metadata.error()));
      source->m_uses_dot_metadata = file_name == "/.metadata";
      source->m_metadata = std::move(*metadata);
      break;
    }
  }
  auto scanned = source->scan(logger, ignore_patterns);
  if (!scanned)
    return std::unexpected(std::move(scanned.error()));
  source->m_asset_paths.sort();
  return source;
}

JsonObject const& DirectoryAssetSource::metadata() const & { return m_metadata; }
std::size_t DirectoryAssetSource::size() const { return m_asset_paths.size(); }
StringList DirectoryAssetSource::asset_paths() const { return m_asset_paths; }
std::span<String const> DirectoryAssetSource::path_view() const & { return {m_asset_paths.data(), m_asset_paths.size()}; }

std::expected<void, StarException> DirectoryAssetSource::visit_paths(PathVisitor const& visit) const {
  for (auto const& path : m_asset_paths)
    if (auto visited = visit(path); !visited)
      return visited;
  return {};
}

bool DirectoryAssetSource::contains(String const& path) const {
  return std::binary_search(m_asset_paths.begin(), m_asset_paths.end(), path);
}

std::expected<StreamOffset, StarException> DirectoryAssetSource::file_size(String const& path) {
  if (!contains(path))
    return std::unexpected(
      AssetSourceException::format("Requested file '{}' does not exist in the directory assets", path));
  auto filesystem = to_filesystem(path);
  if (!filesystem)
    return std::unexpected(std::move(filesystem.error()));
  return File::file_size(*filesystem);
}

std::optional<String> DirectoryAssetSource::missing_file_path(String const& path) const {
  auto filesystem = to_filesystem(path);
  if (!filesystem)
    return {};
  auto native = native_path(*filesystem);
  if (!native)
    return {};
  std::error_code error;
  auto status = std::filesystem::symlink_status(*native, error);
  if (status.type() == std::filesystem::file_type::not_found
      && (!error || error == std::errc::no_such_file_or_directory))
    return std::move(*filesystem);
  return {};
}

std::expected<std::shared_ptr<IODevice>, StarException> DirectoryAssetSource::open(String const& path) {
  auto filesystem = to_filesystem(path);
  if (!filesystem)
    return std::unexpected(std::move(filesystem.error()));
  auto file = File::open(*filesystem, IOMode::Read);
  if (!file)
    return std::unexpected(std::move(file.error()));
  return std::shared_ptr<IODevice>(std::move(*file));
}

std::expected<ByteArray, StarException> DirectoryAssetSource::read(String const& path) {
  auto filesystem = to_filesystem(path);
  if (!filesystem)
    return std::unexpected(std::move(filesystem.error()));
  File file(std::move(*filesystem));
  auto opened = file.open(IOMode::Read);
  if (!opened)
    return std::unexpected(std::move(opened.error()));
  auto size = file.size();
  if (!size)
    return std::unexpected(std::move(size.error()));
  return file.read_bytes(*size);
}

std::expected<String, StarException> DirectoryAssetSource::to_filesystem(String const& path) const {
  auto absolute = path.starts_with("/");
  if (!absolute)
    return std::unexpected(std::move(absolute.error()));
  if (!*absolute)
    return std::unexpected(AssetSourceException::format("Asset path '{}' must be absolute in DirectoryAssetSource::to_filesystem", path));
  auto relative = path.substr(1);
  if (!relative)
    return std::unexpected(std::move(relative.error()));
  return File::relative_to(m_base_directory, File::convert_dir_separators(*relative));
}

std::expected<void, StarException> DirectoryAssetSource::set_metadata(JsonObject metadata) {
  if (metadata != m_metadata) {
    auto filesystem = to_filesystem(m_uses_dot_metadata ? String("/.metadata") : String("/_metadata"));
    if (!filesystem)
      return std::unexpected(std::move(filesystem.error()));
    if (metadata.empty()) {
      auto removed = File::remove(*filesystem);
      if (!removed)
        return std::unexpected(std::move(removed.error()));
    } else {
      auto printed = Json(metadata).print_json(2, true);
      if (!printed)
        return std::unexpected(std::move(printed.error()));
      auto written = File::write_file(*printed, *filesystem);
      if (!written)
        return std::unexpected(std::move(written.error()));
    }
    m_metadata = std::move(metadata);
  }
  return {};
}

std::expected<void, StarException> DirectoryAssetSource::scan(Logger& logger, StringList const& ignore_patterns) {
  auto root = to_filesystem("/");
  if (!root)
    return std::unexpected(std::move(root.error()));
  auto native_root = native_path(*root);
  if (!native_root)
    return std::unexpected(std::move(native_root.error()));
  std::vector<std::filesystem::path> ancestors;
  ancestors.push_back(std::move(*native_root));

  constexpr std::array<std::string_view, 2> metadata_patterns{"^/_metadata$", "^/\\.metadata$"};
  std::vector<CompiledRegex> compiled;
  auto should_ignore = [&](String const& asset_path) -> std::expected<bool, StarException> {
    auto count = ignore_patterns.size() + metadata_patterns.size();
    for (std::size_t i = 0; i != count; ++i) {
      if (i == compiled.size()) {
        // Never compile an unreachable suffix: an earlier match can mask an invalid regex.
        if (i == 0)
          compiled.reserve(count);
        auto pattern = i < ignore_patterns.size()
          ? CompiledRegex::compile(ignore_patterns[i], false)
          : CompiledRegex::compile(String(metadata_patterns[i - ignore_patterns.size()]), false);
        if (!pattern)
          return std::unexpected(std::move(pattern.error()));
        compiled.push_back(std::move(*pattern));
      }
      if (compiled[i].matches(asset_path, false))
        return true;
    }
    return false;
  };

  auto scan_directory = [&](this auto const& recurse, String const& asset_directory,
                            String const& filesystem_directory) -> std::expected<void, StarException> {
    auto entries = File::directory_entries(filesystem_directory);
    if (!entries)
      return std::unexpected(std::move(entries.error()));
    for (auto const& [name, is_directory] : *entries) {
      String asset_path = asset_directory + name;
      if (is_directory) {
        auto filesystem = to_filesystem(asset_path);
        if (!filesystem)
          return std::unexpected(std::move(filesystem.error()));
        auto native = native_path(*filesystem);
        if (!native)
          return std::unexpected(std::move(native.error()));
        // Check only this recursion chain: a sibling alias is a distinct lexical asset path.
        if (std::ranges::any_of(ancestors, [&](auto const& ancestor) {
              std::error_code error;
              return std::filesystem::equivalent(*native, ancestor, error) && !error;
            })) {
          auto warned = logger.warn("DirectoryAssetSource: '{}' resolves to one of its own parent directories, not scanning it", asset_path);
          if (!warned)
            return std::unexpected(std::move(warned.error()));
          continue;
        }
        ancestors.push_back(std::move(*native));
        asset_path += "/";
        auto scanned = recurse(asset_path, *filesystem);
        ancestors.pop_back();
        if (!scanned)
          return scanned;
      } else {
        auto ignored = should_ignore(asset_path);
        if (!ignored)
          return std::unexpected(std::move(ignored.error()));
        if (!*ignored)
          m_asset_paths.push_back(std::move(asset_path));
      }
    }
    return {};
  };
  return scan_directory("/", *root);
}
}
