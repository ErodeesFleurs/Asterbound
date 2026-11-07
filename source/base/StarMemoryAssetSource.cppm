module;
import std;
import star.string;
import star.format;
import star.json;
import star.byte_array;


import star.io_device;
import star.asset_source;
import star.image;
import star.type_identity;

export module star.memory_asset_source;

export namespace Star {

class MemoryAssetSource : public AssetSource {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<MemoryAssetSource, AssetSource>(this, key);
  }

  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<MemoryAssetSource>();
  }

  MemoryAssetSource(String const& name, JsonObject metadata = JsonObject());

  String name() const;
  JsonObject metadata() const override;
  StringList assetPaths() const override;
  [[nodiscard]] std::expected<StreamOffset, StarException> file_size(String const& path) override;

  // do not use the returned IODevice after the file is gone or bad things will happen
  [[nodiscard]] std::expected<std::shared_ptr<IODevice>, StarException> open(String const& path) override;

  bool empty() const;
  bool contains(String const& path) const override;
  bool erase(String const& path);
  void set(String const& path, ByteArray data);
  void set(String const& path, Image const& image);
  void set(String const& path, Image&& image);
  [[nodiscard]] std::expected<ByteArray, StarException> read(String const& path) override;
  [[nodiscard]] std::expected<std::shared_ptr<Image const>, StarException> image(String const& path);
private:
  using FileEntry = std::variant<ByteArray, std::shared_ptr<Image>>;

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

JsonObject MemoryAssetSource::metadata() const {
  return m_metadata;
}

StringList MemoryAssetSource::assetPaths() const {
  return m_files.keys();
}

std::expected<StreamOffset, StarException> MemoryAssetSource::file_size(String const& path) {
  auto entry = m_files.find_ref(path);
  if (!entry)
    return std::unexpected(AssetSourceException::format("Requested file '{}' does not exist in memory", path));
  if (auto bytes = std::get_if<ByteArray>(&entry->get())) {
    if (bytes->size() > static_cast<std::uint64_t>(std::numeric_limits<StreamOffset>::max()))
      return std::unexpected(AssetSourceException::format("Requested file '{}' is too large to open", path));
    return static_cast<StreamOffset>(bytes->size());
  }
  auto const& image = *std::get<std::shared_ptr<Image>>(entry->get());
  std::uint64_t pixels = static_cast<std::uint64_t>(image.width()) * image.height();
  auto bpp = image.bytesPerPixel();
  if (bpp && pixels > static_cast<std::uint64_t>(std::numeric_limits<StreamOffset>::max()) / bpp)
    return std::unexpected(AssetSourceException::format("Requested file '{}' is too large to open", path));
  return static_cast<StreamOffset>(pixels * bpp);
}

std::expected<std::shared_ptr<IODevice>, StarException> MemoryAssetSource::open(String const& path) {
  struct AssetReader : public IODevice {
    AssetReader(std::span<char const> assetData, String name) {
      this->assetData = assetData;
      this->assetSize = assetData.size();
      this->name = std::move(name);
      set_mode(IOMode::Read);
    }

    [[nodiscard]] std::expected<std::size_t, StarException> read(std::span<char> data) override {
      auto len = std::min<std::size_t>(data.size(), assetSize - static_cast<std::size_t>(assetPos));
      std::memcpy(data.data(), assetData.subspan(static_cast<std::size_t>(assetPos), len).data(), len);
      assetPos += len;
      return len;
    }

    [[nodiscard]] std::expected<std::size_t, StarException> write(std::span<char const>) override {
      return std::unexpected(IOException("Assets IODevices are read-only"));
    }

    [[nodiscard]] std::expected<StreamOffset, StarException> size() override { return assetSize; }
    [[nodiscard]] std::expected<StreamOffset, StarException> pos() override { return assetPos; }

    [[nodiscard]] std::expected<String, StarException> device_name() const override { return name; }

    [[nodiscard]] std::expected<bool, StarException> at_end() override {
      return assetPos >= static_cast<StreamOffset>(assetSize);
    }

    [[nodiscard]] std::expected<void, StarException> seek(StreamOffset p, IOSeek mode) override {
      if (mode == IOSeek::Absolute) {
        if (p < 0 || static_cast<std::uint64_t>(p) > assetSize)
          return std::unexpected(IOException("Asset seek position is out of range"));
        assetPos = p;
      } else {
        // Saturate before addition/subtraction to avoid signed overflow.
        StreamOffset base = assetPos;
        bool subtract = mode != IOSeek::Relative;
        if (subtract) {
          if (p > base) assetPos = 0;
          else if (p < base - static_cast<StreamOffset>(assetSize)) assetPos = assetSize;
          else assetPos = base - p;
        } else {
          if (p < -base) assetPos = 0;
          else if (p > static_cast<StreamOffset>(assetSize) - base) assetPos = assetSize;
          else assetPos = base + p;
        }
      }
      return {};
    }

    [[nodiscard]] std::expected<std::shared_ptr<IODevice>, StarException> clone() override {
      auto cloned = std::make_shared<AssetReader>(assetData, name);
      cloned->assetPos = assetPos;
      return cloned;
    }

    std::span<char const> assetData;
    std::size_t assetSize;
    StreamOffset assetPos = 0;
    String name;
  };

  auto p = m_files.find_ref(path);
  if (!p)
    return std::unexpected(AssetSourceException::format("Requested file '{}' does not exist in memory", path));
  else if (std::holds_alternative<ByteArray>(p->get())) {
    auto const& byteArray = std::get<ByteArray>(p->get());
    if (byteArray.size() > static_cast<std::uint64_t>(std::numeric_limits<StreamOffset>::max()))
      return std::unexpected(AssetSourceException::format("Requested file '{}' is too large to open", path));
    return std::make_shared<AssetReader>(byteArray.span(), path);
  } else {
    auto const& image = *std::get<std::shared_ptr<Image>>(p->get());
    std::uint64_t pixels = static_cast<std::uint64_t>(image.width()) * image.height();
    auto bpp = image.bytesPerPixel();
    if (bpp && pixels > static_cast<std::uint64_t>(std::numeric_limits<StreamOffset>::max()) / bpp)
      return std::unexpected(AssetSourceException::format("Requested file '{}' is too large to open", path));
    return std::make_shared<AssetReader>(std::span(reinterpret_cast<char const*>(image.data().data()), pixels * bpp), path);
  }
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
  m_files[path] = std::move(data);
}

void MemoryAssetSource::set(String const& path, Image const& image) {
  m_files[path] = std::make_shared<Image>(image);
}

void MemoryAssetSource::set(String const& path, Image&& image) {
  m_files[path] = std::make_shared<Image>(std::move(image));
}

std::expected<ByteArray, StarException> MemoryAssetSource::read(String const& path) {
  auto p = m_files.find_ref(path);
  if (!p)
    return std::unexpected(AssetSourceException::format("Requested file '{}' does not exist in memory", path));
  else if (std::holds_alternative<ByteArray>(p->get()))
    return std::get<ByteArray>(p->get());
  else {
    auto const& image = *std::get<std::shared_ptr<Image>>(p->get());
    std::uint64_t pixels = static_cast<std::uint64_t>(image.width()) * image.height();
    auto bpp = image.bytesPerPixel();
    if (bpp && pixels > std::numeric_limits<std::size_t>::max() / bpp)
      return std::unexpected(AssetSourceException::format("Requested file '{}' is too large to read", path));
    return ByteArray(std::span(reinterpret_cast<char const*>(image.data().data()), pixels * bpp));
  }
}

std::expected<std::shared_ptr<Image const>, StarException> MemoryAssetSource::image(String const& path) {
  auto p = m_files.find_ref(path);
  if (!p)
    return std::unexpected(AssetSourceException::format("Requested file '{}' does not exist in memory", path));
  else if (std::holds_alternative<std::shared_ptr<Image>>(p->get()))
    return std::get<std::shared_ptr<Image>>(p->get());
  else
    return nullptr;
}

}
