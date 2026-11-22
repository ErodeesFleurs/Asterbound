module;
import std;
import star.format;
import star.json;
import star.byte_array;
import star.ordered_map;
import star.file;
import star.io_device;
import star.asset_source;
import star.string;
import star.directory_asset_source;
import star.data_stream_devices;
import star.data_stream_extra;

export module star.packed_asset_source;

export namespace Star {
class PackedAssetSource final : public AssetSource {
  struct ConstructionKey { explicit ConstructionKey() = default; };

public:
  explicit PackedAssetSource(ConstructionKey, std::shared_ptr<File> file) : m_packed_file(std::move(file)) {}
  using BuildProgressCallback = std::function<std::expected<void, StarException>(
      std::size_t, std::size_t, String const&, String const&)>;

  // Capture each input's extent when opened. Progress is reported after its
  // complete payload has been copied and the input closed, before the next input.
  // Pre-replacement errors preserve the destination. A directory-sync error
  // on Unix is reported after the completed replacement is already visible.
  [[nodiscard]] static std::expected<void, StarException> build(DirectoryAssetSource const& directory_source,
      String const& target, StringList const& extension_sorting = {}, BuildProgressCallback const& progress = {});
  [[nodiscard]] static std::expected<std::shared_ptr<PackedAssetSource>, StarException> create(String const& filename);

  JsonObject const& metadata() const & override { return m_metadata; }
  JsonObject const& metadata() const && = delete;
  std::size_t size() const override { return m_index.size(); }
  StringList asset_paths() const override { return m_index.keys(); }
  bool contains(String const& path) const override;
  [[nodiscard]] std::expected<StreamOffset, StarException> file_size(String const& path) override;
  [[nodiscard]] std::vector<std::expected<StreamOffset, StarException>>
  file_sizes(std::span<std::reference_wrapper<String const> const> paths) override;
  [[nodiscard]] std::expected<std::shared_ptr<IODevice>, StarException> open(String const& path) override;
  [[nodiscard]] std::expected<ByteArray, StarException> read(String const& path) override;

private:
  struct AssetExtent {
    StreamOffset offset;
    StreamOffset size;
  };
  using BackingSize = std::expected<StreamOffset, StarException>;
  [[nodiscard]] std::expected<AssetExtent, StarException> checked_extent(String const& path,
      std::optional<std::reference_wrapper<BackingSize const>> batch_size = {}) const;
  [[nodiscard]] std::expected<void, StarException> visit_paths(PathVisitor const& visit) const override;

  std::shared_ptr<File> m_packed_file;
  JsonObject m_metadata;
  OrderedHashMap<String, std::pair<std::uint64_t, std::uint64_t>> m_index;
};
}

namespace Star {
namespace {
constexpr std::string_view packed_magic = "SBAsset6";
constexpr std::string_view index_magic = "INDEX";
constexpr std::uint64_t payload_start = packed_magic.size() + sizeof(std::uint64_t);

std::expected<void, StarException> write_packed_assets(IODevice& stage, DirectoryAssetSource const& source,
    StringList const& extension_sorting, PackedAssetSource::BuildProgressCallback const& progress) {
  DataStreamIODevice stream(stage);
  if (auto result = stream.write_data(std::span(packed_magic)); !result)
    return result;
  if (auto result = stream.write(std::uint64_t{}); !result)
    return result;

  std::map<std::string, std::size_t, std::less<>> extension_order;
  for (auto const& extension : extension_sorting) {
    auto folded = extension.to_lower();
    if (!folded)
      return std::unexpected(std::move(folded.error()));
    extension_order.try_emplace(std::move(*folded).utf8(), extension_order.size());
  }
  struct Entry {
    String path;
    std::size_t order;
    String folded_path;
  };
  std::vector<Entry> entries;
  auto paths = source.path_view();
  entries.reserve(paths.size());
  for (auto const& path : paths) {
    auto folded = path.to_lower();
    if (!folded)
      return std::unexpected(std::move(folded.error()));
    std::string_view text = folded->utf8();
    auto dot = text.rfind('.');
    auto extension = dot == text.npos ? std::string_view{} : text.substr(dot + 1);
    auto found = extension_order.find(extension);
    entries.push_back({path, found == extension_order.end() ? extension_order.size() : found->second,
                       std::move(*folded)});
  }
  std::sort(entries.begin(), entries.end(), [](Entry const& a, Entry const& b) {
    return std::tie(a.order, a.folded_path) < std::tie(b.order, b.folded_path);
  });

  // Keep the original map type and insertion order for byte-compatible index encoding.
  StringMap<std::pair<std::uint64_t, std::uint64_t>> index;
  std::uint64_t offset = payload_start;
  std::array<char, 64 * 1024> buffer;
  for (std::size_t i = 0; i < entries.size(); ++i) {
    auto& asset_path = entries[i].path;
    auto filename = source.to_filesystem(asset_path);
    if (!filename)
      return std::unexpected(std::move(filename.error()));
    File input(std::move(*filename));
    if (auto result = input.open(IOMode::Read); !result)
      return result;
    auto length = input.size();
    if (!length)
      return std::unexpected(std::move(length.error()));
    if (*length < 0 || static_cast<std::uint64_t>(*length) > std::numeric_limits<StreamOffset>::max() - offset)
      return std::unexpected(AssetSourceException::format("Packed asset '{}' exceeds the representable file extent", asset_path));
    for (auto remaining = *length; remaining > 0;) {
      auto chunk = std::span(buffer).first(static_cast<std::size_t>(std::min<StreamOffset>(remaining, buffer.size())));
      if (auto result = input.read_full(chunk); !result)
        return result;
      if (auto result = stream.write_data(chunk); !result)
        return result;
      remaining -= static_cast<StreamOffset>(chunk.size());
    }
    if (auto result = input.close(); !result)
      return result;
    if (progress) {
      if (auto result = progress(i, entries.size(), input.file_name(), asset_path); !result)
        return result;
    }
    auto size = static_cast<std::uint64_t>(*length);
    auto added = index.add(std::move(asset_path), {offset, size});
    if (!added)
      return std::unexpected(std::move(added.error()));
    offset += size;
  }
  if (auto result = stream.write_data(std::span(index_magic)); !result)
    return result;
  if (auto result = stream.write(source.metadata()); !result)
    return result;
  if (auto result = stream.write(index); !result)
    return result;
  if (auto result = stage.seek(packed_magic.size()); !result)
    return result;
  return stream.write(offset);
}
}

std::expected<void, StarException> PackedAssetSource::build(DirectoryAssetSource const& directory_source,
    String const& target, StringList const& extension_sorting, BuildProgressCallback const& progress) {
  auto produce = [&](IODevice& stage) { return write_packed_assets(stage, directory_source, extension_sorting, progress); };
  return File::write_file_with_rename(target, std::cref(produce));
}

std::expected<std::shared_ptr<PackedAssetSource>, StarException> PackedAssetSource::create(String const& filename) {
  auto opened = File::open(filename, IOMode::Read);
  if (!opened)
    return std::unexpected(std::move(opened.error()));
  auto source = std::make_shared<PackedAssetSource>(ConstructionKey{}, std::move(*opened));
  DataStreamIODevice stream(*source->m_packed_file);
  std::array<char, packed_magic.size()> magic;
  if (auto result = stream.read_data(magic); !result)
    return std::unexpected(std::move(result.error()));
  if (!std::ranges::equal(magic, packed_magic))
    return std::unexpected(AssetSourceException("Packed assets file format unrecognized!"));
  auto index_start = stream.read<std::uint64_t>();
  if (!index_start)
    return std::unexpected(std::move(index_start.error()));
  auto file_size = source->m_packed_file->size();
  if (!file_size)
    return std::unexpected(std::move(file_size.error()));
  if (*file_size < static_cast<StreamOffset>(payload_start + index_magic.size()) || *index_start < payload_start
      || *index_start > static_cast<std::uint64_t>(*file_size) - index_magic.size())
    return std::unexpected(AssetSourceException("Packed assets index pointer is out of range"));
  if (auto result = source->m_packed_file->seek(static_cast<StreamOffset>(*index_start)); !result)
    return std::unexpected(std::move(result.error()));
  std::array<char, index_magic.size()> header;
  if (auto result = stream.read_data(header); !result)
    return std::unexpected(std::move(result.error()));
  if (!std::ranges::equal(header, index_magic))
    return std::unexpected(AssetSourceException("No index header found!"));
  auto metadata = stream.read(source->m_metadata);
  if (!metadata)
    return std::unexpected(std::move(metadata.error()));
  auto index = stream.read(source->m_index);
  if (!index)
    return std::unexpected(std::move(index.error()));
  return source;
}

std::expected<void, StarException> PackedAssetSource::visit_paths(PathVisitor const& visit) const {
  for (auto const& entry : m_index)
    if (auto result = visit(entry.first); !result)
      return result;
  return {};
}

bool PackedAssetSource::contains(String const& path) const { return m_index.contains(path); }

std::expected<PackedAssetSource::AssetExtent, StarException> PackedAssetSource::checked_extent(String const& path,
    std::optional<std::reference_wrapper<BackingSize const>> batch_size) const {
  auto entry = m_index.find_ref(path);
  if (!entry)
    return std::unexpected(
      AssetSourceException::format("Requested file '{}' does not exist in the packed assets file", path));
  auto validate = [&](BackingSize const& backing_size) -> std::expected<AssetExtent, StarException> {
    if (!backing_size)
      return std::unexpected(backing_size.error());
    auto [offset, size] = entry->get();
    if (*backing_size < 0 || offset > static_cast<std::uint64_t>(*backing_size)
        || size > static_cast<std::uint64_t>(*backing_size) - offset)
      return std::unexpected(
        AssetSourceException::format("Requested file '{}' has an out of range size in the packed assets file", path));
    return AssetExtent{static_cast<StreamOffset>(offset), static_cast<StreamOffset>(size)};
  };
  return batch_size ? validate(batch_size->get()) : validate(m_packed_file->size());
}

std::expected<StreamOffset, StarException> PackedAssetSource::file_size(String const& path) {
  return checked_extent(path).transform([](AssetExtent extent) { return extent.size; });
}

std::vector<std::expected<StreamOffset, StarException>>
PackedAssetSource::file_sizes(std::span<std::reference_wrapper<String const> const> paths) {
  std::vector<std::expected<StreamOffset, StarException>> sizes;
  sizes.reserve(paths.size());
  if (paths.empty())
    return sizes;
  auto extent = m_packed_file->size();
  for (auto path : paths)
    sizes.push_back(checked_extent(path.get(), std::cref(extent)).transform([](AssetExtent asset) { return asset.size; }));
  return sizes;
}

std::expected<std::shared_ptr<IODevice>, StarException> PackedAssetSource::open(String const& path) {
  struct AssetReader : public IODevice {
    AssetReader(std::shared_ptr<File> file, String path, StreamOffset offset, BoundedReadCursor cursor)
      : file(std::move(file)), path(std::move(path)), offset(offset), cursor(cursor) {
      set_mode(IOMode::Read);
    }
    [[nodiscard]] std::expected<std::size_t, StarException> read(std::span<char> data) override {
      auto len = cursor.available(data.size());
      auto result = file->read_full_absolute(offset + cursor.pos(), data.first(len));
      if (!result)
        return std::unexpected(std::move(result.error()));
      cursor.advance(len);
      return len;
    }
    [[nodiscard]] std::expected<std::size_t, StarException> write(std::span<char const>) override {
      return std::unexpected(IOException("Assets IODevices are read-only"));
    }
    [[nodiscard]] std::expected<StreamOffset, StarException> size() override { return cursor.size(); }
    [[nodiscard]] std::expected<StreamOffset, StarException> pos() override { return cursor.pos(); }
    [[nodiscard]] std::expected<String, StarException> device_name() const override {
      auto name = file->device_name();
      if (!name)
        return std::unexpected(std::move(name.error()));
      return Star::format("{}:{}", *name, path);
    }
    [[nodiscard]] std::expected<bool, StarException> at_end() override { return cursor.at_end(); }
    [[nodiscard]] std::expected<void, StarException> seek(StreamOffset p, IOSeek mode) override {
      return cursor.seek(p, mode);
    }
    [[nodiscard]] std::expected<std::shared_ptr<IODevice>, StarException> clone() override {
      return std::make_shared<AssetReader>(file, path, offset, cursor);
    }
    std::shared_ptr<File> file;
    String path;
    StreamOffset offset;
    BoundedReadCursor cursor;
  };
  auto extent = checked_extent(path);
  if (!extent)
    return std::unexpected(std::move(extent.error()));
  return std::make_shared<AssetReader>(m_packed_file, path, extent->offset, BoundedReadCursor(extent->size));
}

std::expected<ByteArray, StarException> PackedAssetSource::read(String const& path) {
  auto extent = checked_extent(path);
  if (!extent)
    return std::unexpected(std::move(extent.error()));
  if (static_cast<std::uint64_t>(extent->size) > ByteArray::max_size())
    return std::unexpected(AssetSourceException::format("Requested file '{}' is too large to read", path));
  return m_packed_file->read_bytes_absolute(extent->offset, static_cast<std::size_t>(extent->size));
}
}
