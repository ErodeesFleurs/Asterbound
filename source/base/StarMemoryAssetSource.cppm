module;
import std;
import star.string;
import star.format;
import star.json;
import star.byte_array;
import star.io_device;
import star.asset_source;
import star.image;

export module star.memory_asset_source;

export namespace Star {

class MemoryAssetSource : public AssetSource {
public:
  MemoryAssetSource(String const& name, JsonObject metadata = JsonObject());

  String name() const;
  JsonObject const& metadata() const & override;
  JsonObject const& metadata() const && = delete;
  StringList asset_paths() const override;
  std::size_t size() const override;
  [[nodiscard]] std::expected<StreamOffset, StarException> file_size(String const& path) override;

  // Each reader pins the entry as opened, independently of later edits or source destruction.
  [[nodiscard]] std::expected<std::shared_ptr<IODevice>, StarException> open(String const& path) override;

  bool empty() const;
  bool contains(String const& path) const override;
  bool erase(String const& path);
  void set(String const& path, ByteArray data);
  void set(String const& path, Image const& image);
  void set(String const& path, Image&& image);
  [[nodiscard]] std::expected<ByteArray, StarException> read(String const& path) override;
  [[nodiscard]] std::expected<std::shared_ptr<Image const>, StarException> image(String const& path) override;
private:
  [[nodiscard]] std::expected<void, StarException> visit_paths(PathVisitor const& visit) const override;

  using FileEntry = std::variant<std::shared_ptr<ByteArray const>, std::shared_ptr<Image const>>;
  struct FileView {
    FileEntry const& owner;
    std::span<std::byte const> bytes;
  };
  [[nodiscard]] std::expected<std::reference_wrapper<FileEntry const>, StarException> find_entry(String const& path) const;
  [[nodiscard]] std::expected<FileView, StarException> checked_view(String const& path) const;

  String m_name;
  JsonObject m_metadata;
  StringMap<FileEntry> m_files;
};

}

namespace Star {

MemoryAssetSource::MemoryAssetSource(String const& name, JsonObject metadata)
    : m_name(name), m_metadata(std::move(metadata)) {}

String MemoryAssetSource::name() const {
  return m_name;
}

JsonObject const& MemoryAssetSource::metadata() const & {
  return m_metadata;
}

StringList MemoryAssetSource::asset_paths() const {
  return m_files.keys();
}

std::size_t MemoryAssetSource::size() const {
  return m_files.size();
}

std::expected<void, StarException> MemoryAssetSource::visit_paths(PathVisitor const& visit) const {
  for (auto const& path : m_files.key_view()) {
    if (auto result = visit(path); !result)
      return result;
  }
  return {};
}

auto MemoryAssetSource::find_entry(String const& path) const
    -> std::expected<std::reference_wrapper<FileEntry const>, StarException> {
  if (auto entry = m_files.find_ref(path))
    return *entry;
  return std::unexpected(AssetSourceException::format("Requested file '{}' does not exist in memory", path));
}

auto MemoryAssetSource::checked_view(String const& path) const -> std::expected<FileView, StarException> {
  auto entry = find_entry(path);
  if (!entry)
    return std::unexpected(std::move(entry.error()));
  auto const& owner = entry->get();
  auto bytes = std::holds_alternative<std::shared_ptr<ByteArray const>>(owner)
      ? std::as_bytes(std::get<std::shared_ptr<ByteArray const>>(owner)->span())
      : std::as_bytes(std::get<std::shared_ptr<Image const>>(owner)->span());
  if (!std::in_range<StreamOffset>(bytes.size()))
    return std::unexpected(AssetSourceException::format("Requested file '{}' is too large to open", path));
  return FileView{owner, bytes};
}

std::expected<StreamOffset, StarException> MemoryAssetSource::file_size(String const& path) {
  auto view = checked_view(path);
  if (!view)
    return std::unexpected(std::move(view.error()));
  return static_cast<StreamOffset>(view->bytes.size());
}

std::expected<std::shared_ptr<IODevice>, StarException> MemoryAssetSource::open(String const& path) {
  class AssetReader final : public IODevice {
  public:
    AssetReader(FileView view, String const& name)
        : IODevice(IOMode::Read), m_owner(view.owner), m_bytes(view.bytes),
          m_cursor(static_cast<StreamOffset>(m_bytes.size())), m_name(name) {}

    [[nodiscard]] std::expected<std::size_t, StarException> read(std::span<char> data) override {
      auto count = m_cursor.available(data.size());
      if (count) {
        auto part = m_bytes.subspan(static_cast<std::size_t>(m_cursor.pos()), count);
        std::ranges::copy(part, std::as_writable_bytes(data).begin());
        m_cursor.advance(count);
      }
      return count;
    }

    [[nodiscard]] std::expected<std::size_t, StarException> write(std::span<char const>) override {
      return std::unexpected(IOException("Assets IODevices are read-only"));
    }

    [[nodiscard]] std::expected<StreamOffset, StarException> size() override { return m_cursor.size(); }
    [[nodiscard]] std::expected<StreamOffset, StarException> pos() override { return m_cursor.pos(); }
    [[nodiscard]] std::expected<String, StarException> device_name() const override { return m_name; }
    [[nodiscard]] std::expected<bool, StarException> at_end() override { return m_cursor.at_end(); }
    [[nodiscard]] std::expected<void, StarException> seek(StreamOffset offset, IOSeek mode) override {
      return m_cursor.seek(offset, mode);
    }

    [[nodiscard]] std::expected<std::shared_ptr<IODevice>, StarException> clone() override {
      return std::make_shared<AssetReader>(*this);
    }

  private:
    FileEntry m_owner;
    std::span<std::byte const> m_bytes;
    BoundedReadCursor m_cursor;
    String m_name;
  };

  auto view = checked_view(path);
  if (!view)
    return std::unexpected(std::move(view.error()));
  return std::make_shared<AssetReader>(*view, path);
}

bool MemoryAssetSource::empty() const {
  return m_files.empty();
}

bool MemoryAssetSource::contains(String const& path) const {
  return m_files.contains(path);
}

bool MemoryAssetSource::erase(String const& path) {
  return m_files.erase(path) != 0;
}

void MemoryAssetSource::set(String const& path, ByteArray data) {
  m_files[path] = std::make_shared<ByteArray const>(std::move(data));
}

void MemoryAssetSource::set(String const& path, Image const& image) {
  m_files[path] = std::make_shared<Image const>(image);
}

void MemoryAssetSource::set(String const& path, Image&& image) {
  m_files[path] = std::make_shared<Image const>(std::move(image));
}

std::expected<ByteArray, StarException> MemoryAssetSource::read(String const& path) {
  auto view = checked_view(path);
  if (!view)
    return std::unexpected(std::move(view.error()));
  ByteArray bytes;
  bytes.resize(view->bytes.size());
  if (!bytes.empty())
    std::ranges::copy(view->bytes, std::as_writable_bytes(bytes.span()).begin());
  return bytes;
}

std::expected<std::shared_ptr<Image const>, StarException> MemoryAssetSource::image(String const& path) {
  auto entry = find_entry(path);
  if (!entry)
    return std::unexpected(std::move(entry.error()));
  if (std::holds_alternative<std::shared_ptr<Image const>>(entry->get()))
    return std::get<std::shared_ptr<Image const>>(entry->get());
  return std::shared_ptr<Image const>{};
}

}
