module;
import std;
import star.memory;
import star.format;
import star.json;
import star.byte_array;
import star.ordered_map;
import star.file;
import star.io_device;
import star.asset_source;
import star.string;
import star.directory_asset_source;
import star.ordered_set;
import star.data_stream_devices;
import star.data_stream_extra;
import star.math_common;
import star.type_identity;

export module star.packed_asset_source;

export namespace Star {
class PackedAssetSource : public AssetSource {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<PackedAssetSource, AssetSource>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<PackedAssetSource>();
  }
  using BuildProgressCallback = std::function<void(std::size_t, std::size_t, String, String)>;

  // Build a packed file, sorting specified extensions before all others and
  // sorting names case-insensitively within each extension group.
  [[nodiscard]] static std::expected<void, StarException> build(DirectoryAssetSource& directorySource, String const& targetPackedFile,
      StringList const& extensionSorting = {}, BuildProgressCallback progressCallback = {});
  [[nodiscard]] static std::expected<std::shared_ptr<PackedAssetSource>, StarException> create(String const& packedFileName);

  JsonObject metadata() const override;
  StringList assetPaths() const override;
  bool contains(String const& path) const override;
  [[nodiscard]] std::expected<std::shared_ptr<IODevice>, StarException> open(String const& path) override;
  [[nodiscard]] std::expected<ByteArray, StarException> read(String const& path) override;

private:
  PackedAssetSource() = default;
  std::shared_ptr<File> m_packedFile;
  JsonObject m_metadata;
  OrderedHashMap<String, std::pair<std::uint64_t, std::uint64_t>> m_index;
};
}

namespace Star {
std::expected<void, StarException> PackedAssetSource::build(DirectoryAssetSource& directorySource,
    String const& targetPackedFile, StringList const& extensionSorting, BuildProgressCallback progressCallback) {
  auto opened = File::open(targetPackedFile, IOMode::ReadWrite | IOMode::Truncate);
  if (!opened)
    return std::unexpected(std::move(opened.error()));
  auto file = std::move(*opened);
  DataStreamIODevice ds(*file);
  auto magic = ds.writeData(std::span("SBAsset6", 8));
  if (!magic)
    return std::unexpected(std::move(magic.error()));
  auto skipped = ds.seek(8, IOSeek::Relative);
  if (!skipped)
    return std::unexpected(std::move(skipped.error()));
  StringMap<std::pair<std::uint64_t, std::uint64_t>> index;
  OrderedHashSet<String> extensionOrdering;
  for (auto const& str : extensionSorting) {
    auto folded = str.toLower();
    if (!folded)
      return std::unexpected(std::move(folded.error()));
    extensionOrdering.add(std::move(*folded));
  }

  struct Entry {
    String path;
    std::size_t extensionOrder;
    String foldedPath;
  };
  auto assetPaths = directorySource.assetPaths();
  std::vector<Entry> entries;
  entries.reserve(assetPaths.size());
  for (auto const& path : assetPaths) {
    String extension;
    auto lastDot = path.findLast(".");
    if (!lastDot)
      return std::unexpected(std::move(lastDot.error()));
    if (*lastDot != NPos) {
      auto suffix = path.substr(*lastDot + 1);
      if (!suffix)
        return std::unexpected(std::move(suffix.error()));
      extension = std::move(*suffix);
    }
    auto foldedExtension = extension.toLower();
    if (!foldedExtension)
      return std::unexpected(std::move(foldedExtension.error()));
    auto order = extensionOrdering.indexOf(*foldedExtension);
    auto foldedPath = path.toLower();
    if (!foldedPath)
      return std::unexpected(std::move(foldedPath.error()));
    entries.push_back({.path = path,
                       .extensionOrder = order ? *order : extensionOrdering.size(),
                       .foldedPath = std::move(*foldedPath)});
  }
  std::sort(entries.begin(), entries.end(), [](Entry const& a, Entry const& b) {
    return std::tie(a.extensionOrder, a.foldedPath) < std::tie(b.extensionOrder, b.foldedPath);
  });
  for (std::size_t i = 0; i < entries.size(); ++i) {
    auto const& assetPath = entries[i].path;
    auto contents = directorySource.read(assetPath);
    if (!contents)
      return std::unexpected(std::move(contents.error()));
    if (progressCallback) {
      auto filesystem = directorySource.toFilesystem(assetPath);
      if (!filesystem)
        return std::unexpected(std::move(filesystem.error()));
      progressCallback(i, entries.size(), std::move(*filesystem), assetPath);
    }
    auto position = ds.pos();
    if (!position)
      return std::unexpected(std::move(position.error()));
    auto added = index.add(assetPath, {static_cast<std::uint64_t>(*position), contents->size()});
    if (!added)
      return std::unexpected(std::move(added.error()));
    auto written = ds.writeBytes(*contents);
    if (!written)
      return std::unexpected(std::move(written.error()));
  }
  auto indexPosition = ds.pos();
  if (!indexPosition)
    return std::unexpected(std::move(indexPosition.error()));
  std::uint64_t indexStart = *indexPosition;
  auto header = ds.writeData(std::span("INDEX", 5));
  if (!header)
    return std::unexpected(std::move(header.error()));
  auto metadata = ds.write(directorySource.metadata());
  if (!metadata)
    return std::unexpected(std::move(metadata.error()));
  auto writtenIndex = ds.write(index);
  if (!writtenIndex)
    return std::unexpected(std::move(writtenIndex.error()));
  auto rewound = ds.seek(8);
  if (!rewound)
    return std::unexpected(std::move(rewound.error()));
  auto pointer = ds.write(indexStart);
  if (!pointer)
    return std::unexpected(std::move(pointer.error()));
  return file->close();
}

std::expected<std::shared_ptr<PackedAssetSource>, StarException> PackedAssetSource::create(String const& filename) {
  auto opened = File::open(filename, IOMode::Read);
  if (!opened)
    return std::unexpected(std::move(opened.error()));
  auto source = std::shared_ptr<PackedAssetSource>(new PackedAssetSource());
  source->m_packedFile = std::move(*opened);
  DataStreamIODevice ds(*source->m_packedFile);
  auto magic = ds.readBytes(8);
  if (!magic)
    return std::unexpected(std::move(magic.error()));
  if (*magic != ByteArray(std::span("SBAsset6", 8)))
    return std::unexpected(AssetSourceException("Packed assets file format unrecognized!"));
  auto indexStart = ds.read<std::uint64_t>();
  if (!indexStart)
    return std::unexpected(std::move(indexStart.error()));
  auto fileSize = source->m_packedFile->size();
  if (!fileSize)
    return std::unexpected(std::move(fileSize.error()));
  if (*fileSize < 21 || *indexStart < 16 || *indexStart > static_cast<std::uint64_t>(*fileSize - 5))
    return std::unexpected(AssetSourceException("Packed assets index pointer is out of range"));
  auto sought = ds.seek(static_cast<StreamOffset>(*indexStart));
  if (!sought)
    return std::unexpected(std::move(sought.error()));
  auto header = ds.readBytes(5);
  if (!header)
    return std::unexpected(std::move(header.error()));
  if (*header != ByteArray(std::span("INDEX", 5)))
    return std::unexpected(AssetSourceException("No index header found!"));
  auto metadata = ds.read(source->m_metadata);
  if (!metadata)
    return std::unexpected(std::move(metadata.error()));
  auto index = ds.read(source->m_index);
  if (!index)
    return std::unexpected(std::move(index.error()));
  return source;
}

JsonObject PackedAssetSource::metadata() const { return m_metadata; }
StringList PackedAssetSource::assetPaths() const { return m_index.keys(); }
bool PackedAssetSource::contains(String const& path) const { return m_index.contains(path); }

std::expected<std::shared_ptr<IODevice>, StarException> PackedAssetSource::open(String const& path) {
  struct AssetReader : public IODevice {
    AssetReader(std::shared_ptr<File> file, String path, StreamOffset offset, StreamOffset size)
      : file(std::move(file)), path(std::move(path)), fileOffset(offset), assetSize(size) {
      setMode(IOMode::Read);
    }
    [[nodiscard]] std::expected<std::size_t, StarException> read(std::span<char> data) override {
      auto len = std::min<std::uint64_t>(data.size(), assetSize - assetPos);
      auto result = file->readFullAbsolute(fileOffset + assetPos, data.first(len));
      if (!result)
        return std::unexpected(std::move(result.error()));
      assetPos += len;
      return len;
    }
    [[nodiscard]] std::expected<std::size_t, StarException> write(std::span<char const>) override {
      return std::unexpected(IOException("Assets IODevices are read-only"));
    }
    [[nodiscard]] std::expected<StreamOffset, StarException> size() override { return assetSize; }
    [[nodiscard]] std::expected<StreamOffset, StarException> pos() override { return assetPos; }
    [[nodiscard]] std::expected<String, StarException> deviceName() const override {
      auto name = file->deviceName();
      if (!name)
        return std::unexpected(std::move(name.error()));
      return strf("{}:{}", *name, path);
    }
    [[nodiscard]] std::expected<bool, StarException> atEnd() override { return assetPos >= assetSize; }
    [[nodiscard]] std::expected<void, StarException> seek(StreamOffset p, IOSeek mode) override {
      if (mode == IOSeek::Absolute) {
        if (p < 0 || p > assetSize)
          return std::unexpected(IOException("Asset seek position is out of range"));
        assetPos = p;
      } else if (mode == IOSeek::Relative) {
        if (p < -assetPos) assetPos = 0;
        else if (p > assetSize - assetPos) assetPos = assetSize;
        else assetPos += p;
      } else {
        if (p > assetSize) assetPos = 0;
        else if (p < 0) assetPos = assetSize;
        else assetPos = assetSize - p;
      }
      return {};
    }
    [[nodiscard]] std::expected<std::shared_ptr<IODevice>, StarException> clone() override {
      auto cloned = std::make_shared<AssetReader>(file, path, fileOffset, assetSize);
      cloned->assetPos = assetPos;
      return cloned;
    }
    std::shared_ptr<File> file;
    String path;
    StreamOffset fileOffset;
    StreamOffset assetSize;
    StreamOffset assetPos = 0;
  };
  auto entry = m_index.find_ref(path);
  if (!entry)
    return std::unexpected(AssetSourceException::format("Requested file '{}' does not exist in the packed assets file", path));
  auto fileSize = m_packedFile->size();
  if (!fileSize)
    return std::unexpected(std::move(fileSize.error()));
  if (*fileSize < 0 || entry->get().first > static_cast<std::uint64_t>(*fileSize)
      || entry->get().second > static_cast<std::uint64_t>(*fileSize) - entry->get().first)
    return std::unexpected(AssetSourceException::format("Requested file '{}' has an out of range size in the packed assets file", path));
  return std::make_shared<AssetReader>(m_packedFile, path, entry->get().first, entry->get().second);
}

std::expected<ByteArray, StarException> PackedAssetSource::read(String const& path) {
  auto device = open(path);
  if (!device)
    return std::unexpected(std::move(device.error()));
  auto size = (*device)->size();
  if (!size)
    return std::unexpected(std::move(size.error()));
  if (static_cast<std::uint64_t>(*size) > std::numeric_limits<std::size_t>::max())
    return std::unexpected(AssetSourceException::format("Requested file '{}' is too large to read", path));
  return (*device)->readBytes(static_cast<std::size_t>(*size));
}
}
