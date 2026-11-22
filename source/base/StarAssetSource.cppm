module;
import std;
import star.byte_array;

import star.string;
import star.format;
import star.json;
import star.io_device;

namespace Star {

struct AssetSourceExceptionTag {
  static constexpr std::string_view name() { return "AssetSourceException"; }
};
using AssetSourceException = StarError<AssetSourceExceptionTag, StarException>;

class Image;

// Shared cursor for read-only slices. End offsets count backwards from size.
class BoundedReadCursor {
public:
  explicit BoundedReadCursor(StreamOffset size) : m_size(size) {}

  StreamOffset size() const noexcept { return m_size; }
  StreamOffset pos() const noexcept { return m_position; }
  bool at_end() const noexcept { return m_position == m_size; }
  std::size_t available(std::size_t request) const noexcept {
    return std::min<std::uint64_t>(request, m_size - m_position);
  }
  // The caller advances only after successfully reading at most available(request).
  void advance(std::size_t consumed) noexcept { m_position += static_cast<StreamOffset>(consumed); }
  [[nodiscard]] std::expected<void, StarException> seek(StreamOffset offset, IOSeek mode) {
    if (mode == IOSeek::Absolute) {
      if (offset < 0 || offset > m_size)
        return std::unexpected(IOException("Asset seek position is out of range"));
      m_position = offset;
    } else if (mode == IOSeek::Relative) {
      m_position += std::clamp(offset, -m_position, m_size - m_position);
    } else {
      m_position = m_size - std::clamp<StreamOffset>(offset, 0, m_size);
    }
    return {};
  }

private:
  StreamOffset m_size;
  StreamOffset m_position = 0;
};

template <typename Visitor>
concept asset_path_visitor = requires(Visitor& visit, String const& path) {
  { std::invoke(visit, path) } -> std::same_as<std::expected<void, StarException>>;
};

// An asset source could be a directory on a filesystem, where assets are
// pulled directly from files, or a single pak-like file containing all assets,
// where assets are pulled from the correct region of the pak-like file.
class AssetSource {
public:
  virtual ~AssetSource() = default;

  // Synchronous invocation; borrowed paths cannot be retained past the call.
  using PathVisitor = std::function<std::expected<void, StarException>(String const&)>;

  // An asset source can have arbitrary metadata attached.
  virtual JsonObject const& metadata() const & = 0;
  JsonObject const& metadata() const && = delete;

  // The snapshot survives source mutation; the visitor must not mutate the source.
  virtual StringList asset_paths() const = 0;
  virtual std::size_t size() const = 0;
  template <asset_path_visitor Visitor>
  [[nodiscard]] std::expected<void, StarException> for_each_path(Visitor&& visit) const {
    if constexpr (std::same_as<std::remove_cvref_t<Visitor>, PathVisitor> || std::is_pointer_v<std::remove_reference_t<Visitor>>)
      if (!visit)
        return std::unexpected(AssetSourceException("Empty asset path visitor"));
    return visit_paths(std::ref(visit));
  }

  // Optional provider capabilities. A null image falls back to stream decoding.
  [[nodiscard]] virtual std::expected<std::shared_ptr<Image const>, StarException> image(String const&) {
    return std::shared_ptr<Image const>{};
  }
  // Only confirmed vanished loose files may be ignored by the patch loader.
  virtual std::optional<String> missing_file_path(String const&) const { return {}; }

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
private:
  virtual std::expected<void, StarException> visit_paths(PathVisitor const& visit) const = 0;
};

}

export module star.asset_source;

export namespace Star {
  using ::Star::AssetSourceExceptionTag;
  using ::Star::AssetSourceException;
  using ::Star::AssetSource;
  using ::Star::BoundedReadCursor;
  using ::Star::asset_path_visitor;
}
