module;
import std;
import star.string;
import star.map;
import star.format;
import star.json;
import star.byte_array;
import star.ordered_map;
import star.rect;
import star.bi_map;
import star.thread;
import star.io_device;
import star.list;
import star.image;
import star.vector;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ref_ptr;

import star.asset_source;
import star.font;

namespace Star {

class Logger;
class RandomService;
class LogMap;

class Audio;
class Image;

class LuaContext;
class LuaEngine;

struct AssetExceptionTag {
  static constexpr std::string_view name() { return "AssetException"; }
};
using AssetException = StarError<AssetExceptionTag, StarException>;

// The contents of an assets .frames file, which can be associated with one or
// more images, and specifies named sub-rects of those images.
struct FramesSpecification {
  // Get the target sub-rect of a given frame name (which can be an alias).
  // Returns nothing if the frame name is not found.
  [[nodiscard]] std::expected<std::optional<RectU>, StarException> get_rect(String const& frame) const;
  // Converts to Json.
  [[nodiscard]] std::expected<Json, StarException> to_json() const;
  // The full path to the .frames file from which this was loaded.
  String frames_file;
  // Named sub-frames
  StringMap<RectU> frames;
  // Aliases for named sub-frames, always points to a valid frame name in the
  // 'frames' map.
  StringMap<String> aliases;
};

template <typename Range>
concept asset_path_range = std::ranges::input_range<Range const>
    && (std::same_as<std::remove_cvref_t<std::ranges::range_reference_t<Range const>>, String>
        || std::same_as<std::remove_cvref_t<std::ranges::range_reference_t<Range const>>, CaseInsensitiveString>);

// The assets system can load image, font, json, and data assets from a set of
// sources.  Each source is either a directory on the filesystem or a single
// packed asset file.
//
// Assets is thread safe and performs TTL caching.
class Assets {
public:
  struct Settings {
    // TTL for cached assets
    float asset_time_to_live;

    // Audio under this length will be automatically decompressed
    float audio_decompress_limit;

    // Number of background worker threads
    unsigned worker_pool_size;

    // If given, if an image is unable to load, will log the error and load
    // this path instead
    std::optional<String> missing_image;

    // Same, but for audio
    std::optional<String> missing_audio;

    // When loading assets from a directory, will automatically ignore any
    // files whose asset paths matching any of the given patterns.
    StringList path_ignore;

    // Same, but only ignores the file for the purposes of calculating the
    // digest.
    StringList digest_ignore;

    // Longer lifetime for recently referenced blend/mask images. The pixel
    // budget limits this extension, not total cache or externally held memory.
    float reference_image_time_to_live = 300;
    std::uint64_t reference_image_pixel_budget = 16 * 1024 * 1024;

    // Remote paths contain data, never scripts or implicitly readable bytes.
    bool allow_remote_assets = true;
  };

  enum class QueuePriority {
    None,
    Working,
    PostProcess,
    Load
  };

  enum class AssetType {
    Json,
    Image,
    Audio,
    Font,
    Bytes
  };

  struct AssetId {
    AssetType type;
    AssetPath path;

    bool operator==(AssetId const& asset_id) const;
  };

  struct AssetIdHash {
    std::size_t operator()(AssetId const& id) const;
  };

  struct AssetData {
    virtual ~AssetData() = default;

    // Decoders are selected by AssetId::type. Keep checked access local to this closed set.
    AssetType const type;
    explicit AssetData(AssetType kind) noexcept : type(kind) {}

    // Should return true if this asset is shared and still in use, so freeing
    // it from cache will not really free the resource, so it should persist in
    // the cache.
    virtual bool should_persist() const = 0;

    // Keep absolute seconds at double precision before computing cache ages.
    std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>> time{};
    bool needs_post_processing = false;
    bool force_persist = false;
    // Recomputed under the cache mutex for each reference-image budget pass.
    bool extended_ttl = false;
  };

  struct JsonData final : AssetData {
    static constexpr AssetType kind = AssetType::Json;
    JsonData() noexcept : AssetData(kind) {}

    bool should_persist() const override;

    Json json;
  };

  // Image data for an image, sub-frame, or post-processed image.
  struct ImageData final : AssetData {
    static constexpr AssetType kind = AssetType::Image;
    ImageData() noexcept : AssetData(kind) {}

    bool should_persist() const override;

    std::shared_ptr<Image const> image;

    // *Optional* sub-frames data for this image, only will exist when the
    // image is a top-level image and has an associated frames file.
    std::shared_ptr<FramesSpecification const> frames;

    // If this image aliases another asset entry, this will be true and
    // should_persist will never be true (to ensure that this alias and its
    // target can be removed from the cache).
    bool alias = false;

    bool is_reference = false;
  };

  struct AudioData final : AssetData {
    static constexpr AssetType kind = AssetType::Audio;
    AudioData() noexcept : AssetData(kind) {}

    bool should_persist() const override;

    std::shared_ptr<Audio const> audio;
  };

  struct FontData final : AssetData {
    static constexpr AssetType kind = AssetType::Font;
    FontData() noexcept : AssetData(kind) {}

    bool should_persist() const override;

    std::shared_ptr<Font const> font;
  };

  struct BytesData final : AssetData {
    static constexpr AssetType kind = AssetType::Bytes;
    BytesData() noexcept : AssetData(kind) {}

    bool should_persist() const override;

    std::shared_ptr<ByteArray const> bytes;
  };

  struct AssetFileDescriptor {
    // The mixed case original source name;
    String source_name;
    // The source that has the primary asset copy
    std::shared_ptr<AssetSource> source;
    // List of source names and sources for patches to this file.
    List<std::pair<String, std::shared_ptr<AssetSource>>> patch_sources;
  };

  [[nodiscard]] static std::expected<std::shared_ptr<Assets>, StarException> create(Logger& logger, RandomService& random, LogMap& log_map, Settings settings, StringList asset_sources);
  ~Assets();

  void hot_reload() const;

  // Returns a list of all the asset source paths used by Assets in load order.
  StringList asset_sources() const;

  // Return metadata for the given loaded asset source path
  [[nodiscard]] std::expected<JsonObject, StarException> asset_source_metadata(String const& source_path) const;

  // Exact metadata name lookup in load order, excluding generated providers.
  // The returned path is borrowed from this Assets generation.
  [[nodiscard]] std::optional<std::reference_wrapper<String const>> asset_source_path_for_name(String const& name) const &;
  std::optional<std::reference_wrapper<String const>> asset_source_path_for_name(String const&) const && = delete;

  // An imperfect sha256 digest of the contents of all combined asset sources.
  // Useful for detecting if there are mismatched assets between a client and
  // server or if assets sources have changed from a previous load.
  ByteArray digest() const;

  // Is there an asset associated with the given path?  Path must not contain
  // sub-paths or directives.
  [[nodiscard]] std::expected<bool, StarException> asset_exists(String const& path) const;

  [[nodiscard]] std::expected<std::optional<AssetFileDescriptor>, StarException> asset_descriptor(String const& path) const;

  // The name of the asset source within which the path exists.
  [[nodiscard]] std::expected<String, StarException> asset_source(String const& path) const;

  std::optional<String> asset_source_path(std::shared_ptr<AssetSource> const& source) const;

  // Describe a held provider without resolving a possibly overridden virtual path.
  // Missing or invalid optional metadata falls back to the physical source path.
  static std::string source_diagnostic(AssetSource const& source,
      std::optional<std::reference_wrapper<String const>> source_path);
  StarException with_asset_context(StarException error, String const& path, std::string_view operation) const;

  // Scans for all assets with the given suffix in any directory.
  [[nodiscard]] std::expected<StringList, StarException> scan(String const& suffix) const;
  // Scans for all assets matching both prefix and suffix (prefix may be, for
  // example, a directory)
  [[nodiscard]] std::expected<StringList, StarException> scan(String const& prefix, String const& suffix) const;
  // Scans all assets for files with the given extension, which is specially
  // indexed and much faster than a normal scan.  Extension may contain leading
  // '.' character or it may be omitted.
  [[nodiscard]] std::expected<std::reference_wrapper<CaseInsensitiveStringSet const>, StarException> scan_extension(String const& extension) const;

  // Get json asset with an optional sub-path.  The sub-path portion of the
  // path refers to a key in the top-level object, and may use dot notation
  // for deeper field access and [] notation for array access.  Example:
  // "/path/to/json:key1.key2.key3[4]".
  [[nodiscard]] std::expected<Json, StarException> json(String const& path) const;

  // Either returns the json v, or, if v is a string type, returns the json
  // pointed to by interpreting v as a string path.
  [[nodiscard]] std::expected<Json, StarException> fetch_json(Json const& v, String const& dir = "/") const;

  // Batch queues validate the entire range before publishing any work.
  // JSON selectors preload just the base document.
  [[nodiscard]] std::expected<void, StarException> queue_jsons(asset_path_range auto const& paths) const {
    return queue_batch(AssetType::Json, paths);
  }

  // Returns *either* an image asset or a sub-frame.  Frame files are JSON
  // descriptor files that reference a particular image and label separate
  // sub-rects of the image.  If the given path has a ':' sub-path, then the
  // assets system will look for an associated .frames named either
  // <full-path-minus-extension>.frames or default.frames, going up to assets
  // root.  May return the same ImageConstPtr for different paths if the paths
  // are equivalent or they are aliases of other image paths.
  [[nodiscard]] std::expected<std::shared_ptr<Image const>, StarException> image(AssetPath const& path) const;
  // For an unqualified path, inspect its stored Image or PNG header without
  // executing patches. No hint means callers must use the normal image loader.
  [[nodiscard]] std::expected<std::optional<std::pair<Vec2U, PixelFormat>>, StarException>
  image_size_hint(String const& path) const;
  // Load images using background processing.
  [[nodiscard]] std::expected<void, StarException> queue_images(asset_path_range auto const& paths) const {
    return queue_batch(AssetType::Image, paths);
  }
  // A successful null result means the image was queued for loading.
  [[nodiscard]] std::expected<std::shared_ptr<Image const>, StarException> try_image(AssetPath const& path) const;

  // Returns the best associated FramesSpecification for a given image path, if
  // it exists.  The given path must not contain sub-paths or directives, and
  // this function may return nullptr if no frames file is associated with the
  // given image path.
  [[nodiscard]] std::expected<std::shared_ptr<FramesSpecification const>, StarException> image_frames(String const& path) const;

  [[nodiscard]] std::expected<std::shared_ptr<Audio const>, StarException> audio(String const& path) const;
  [[nodiscard]] std::expected<void, StarException> queue_audios(asset_path_range auto const& paths) const {
    return queue_batch(AssetType::Audio, paths);
  }
  // A successful null result means the audio was queued for loading.
  [[nodiscard]] std::expected<std::shared_ptr<Audio const>, StarException> try_audio(String const& path) const;

  // Returns pointer to shared font asset
  [[nodiscard]] std::expected<std::shared_ptr<Font const>, StarException> font(String const& path) const;

  // Embedded bytes require explicit permission, even after a cache hit.
  [[nodiscard]] std::expected<std::shared_ptr<ByteArray const>, StarException> bytes(String const& path, bool allow_remote = false) const;

  // Bypass asset caching and open an asset file directly.
  [[nodiscard]] std::expected<std::shared_ptr<IODevice>, StarException> open_file(String const& base_path) const;

  // Clear all cached assets that are not queued, persistent, or broken.
  void clear_cache();

  // Run a cleanup pass and remove any assets past their time to live.
  void cleanup();

private:
  friend struct AssetWorkerBenchmarkAccess;
  friend struct AssetWorkerTestAccess;

  explicit Assets(Logger& logger);
  [[nodiscard]] std::expected<void, StarException> initialize(RandomService& random, LogMap& log_map, Settings settings, StringList asset_sources);

  [[nodiscard]] static std::expected<FramesSpecification, StarException> parse_frames_specification(Json const& frame_config, String path);

  static String const& queue_path(String const& path) { return path; }
  static String const& queue_path(CaseInsensitiveString const& path) { return path.string(); }
  [[nodiscard]] static std::expected<AssetId, StarException> prepare_queued_asset(AssetType type, String const& path);
  template <asset_path_range Range>
  [[nodiscard]] std::expected<void, StarException> queue_batch(AssetType type, Range const& paths) const {
    List<AssetId> ids;
    if constexpr (std::ranges::sized_range<Range const>)
      ids.reserve(std::ranges::size(paths));
    for (auto const& path : paths) {
      auto id = prepare_queued_asset(type, queue_path(path));
      if (!id)
        return std::unexpected(std::move(id.error()));
      ids.push_back(std::move(*id));
    }
    return ids.empty() ? std::expected<void, StarException>{} : publish_queued_assets(ids);
  }
  [[nodiscard]] std::expected<void, StarException> publish_queued_assets(std::span<AssetId> ids) const;
  [[nodiscard]] std::expected<std::shared_ptr<AssetData>, StarException> try_asset(AssetId const& id) const;
  [[nodiscard]] std::expected<std::shared_ptr<AssetData>, StarException> get_asset(AssetId const& id, bool persist = false) const;

  [[nodiscard]] std::expected<void, StarException> worker_main(std::stop_token stop_token);
  [[nodiscard]] std::expected<void, StarException> stop_workers();

  // All methods below assume that the asset mutex is locked when calling.

  // Do some processing that might take a long time and should not hold the
  // assets mutex during it. Unlocks for the callback and re-locks before return.
  template <typename Function> requires std::invocable<Function&>
  [[nodiscard]] auto unlock_during(Function&& function) const;

  // Returns the best frames specification for the given image path, if it exists.
  [[nodiscard]] std::expected<std::shared_ptr<FramesSpecification const>, StarException> best_frames_specification(String const& image) const;
  struct FrameFile {
    String path; // Empty only for a completed catalog miss, never for an I/O or parse error.
    bool shareable = false;
  };
  [[nodiscard]] std::expected<FrameFile, StarException> find_frame_file(String const& image) const;
  void clear_frame_catalog_cache() const;
  [[nodiscard]] std::expected<void, StarException> invalidate_frame_file(String const& path) const;

  [[nodiscard]] std::expected<std::shared_ptr<IODevice>, StarException> open(String const& base_path) const;
  [[nodiscard]] std::expected<ByteArray, StarException> read(String const& base_path) const;
  [[nodiscard]] std::expected<std::shared_ptr<Image const>, StarException> read_image(String const& path) const;
  [[nodiscard]] std::expected<std::shared_ptr<Image const>, StarException> apply_image_patches(std::shared_ptr<Image const> image, String const& path, List<std::pair<String, std::shared_ptr<AssetSource>>> patches) const;

  [[nodiscard]] std::expected<Json, StarException> read_json(String const& base_path) const;
  [[nodiscard]] std::expected<Json, StarException> apply_json_patches(Json const& input, String const& path, List<std::pair<String, std::shared_ptr<AssetSource>>> patches) const;
  [[nodiscard]] std::expected<void, StarException> report_patch_rejection(String const& path,
      std::shared_ptr<AssetSource> const& source, StarException const& error) const;
  [[nodiscard]] std::expected<Json, StarException> check_patch_array(String const& path,
                                                                   std::shared_ptr<AssetSource> const& source,
                                                                   Json const result, JsonArray const& patch_data,
                                                                   std::optional<Json> const external) const;

  // Load / post process an asset and log any failure. Returns true if the
  // work was performed (whether successful or not), false if the work is
  // blocking on something.
  [[nodiscard]] std::expected<bool, StarException> do_load(AssetId const& id) const;
  struct PostTask {
    std::uint64_t epoch;
    std::shared_ptr<AssetData> original;
  };
  struct WorkerTask {
    AssetId id;
    std::optional<PostTask> post;
  };
  [[nodiscard]] std::optional<WorkerTask> next_worker_task() const;
  [[nodiscard]] std::expected<bool, StarException> publish_post(AssetId const& id, PostTask task,
      std::expected<std::shared_ptr<AssetData>, StarException> processed) const;

  // A successful null result means loading is blocked by another busy asset.
  [[nodiscard]] std::expected<std::shared_ptr<AssetData>, StarException> load_asset(AssetId const& id) const;
  void publish_asset_failure(AssetId const& id, StarException const& error) const;
  [[nodiscard]] std::uint64_t current_load_epoch() const;

  [[nodiscard]] std::expected<std::shared_ptr<AssetData>, StarException> load_json(AssetPath const& path) const;
  [[nodiscard]] std::expected<std::shared_ptr<AssetData>, StarException> load_image(AssetPath const& path) const;
  [[nodiscard]] std::expected<std::shared_ptr<AssetData>, StarException> load_audio(AssetPath const& path) const;
  [[nodiscard]] std::expected<std::shared_ptr<AssetData>, StarException> load_font(AssetPath const& path) const;
  [[nodiscard]] std::expected<std::shared_ptr<AssetData>, StarException> load_bytes(AssetPath const& path) const;

  // The claimed task owns its readable original; decode without the asset mutex.
  [[nodiscard]] std::expected<std::shared_ptr<AssetData>, StarException> post_process_audio(PostTask const& task) const;

  // Updates the asset's monotonic access time.
  void freshen(std::shared_ptr<AssetData> const& asset) const;

  Logger& m_logger;
  std::optional<FontLibrary> m_font_library;

  Settings m_settings;

  mutable std::mutex m_assets_mutex;
  mutable std::uint64_t m_load_epoch = 0;

  mutable std::condition_variable m_assets_queued;
  mutable OrderedHashMap<AssetId, QueuePriority, AssetIdHash> m_queue;
  // Protected by m_assetsMutex. One worker claims collection for a drained
  // load burst; in-flight work must not compete with idle full collections.
  mutable bool m_lua_gc_pending = true;

  mutable std::condition_variable m_assets_done;
  mutable HashMap<AssetId, std::shared_ptr<AssetData>, AssetIdHash> m_assets_cache;
  mutable HashMap<AssetId, StarException, AssetIdHash> m_asset_errors;

  mutable StringMap<FrameFile> m_frame_files_by_image;
  mutable StringMap<std::shared_ptr<FramesSpecification const>> m_frames_by_file;
  mutable StringMap<std::shared_ptr<FramesSpecification const>> m_patched_frames_by_image;

  // Lua
  struct PatchContextKey {
    std::reference_wrapper<AssetSource> source;
    String path;

    bool operator==(PatchContextKey const& other) const {
      // Native addresses identify sources; asset contents do not define identity.
      return std::addressof(source.get()) == std::addressof(other.source.get()) && path == other.path;
    }
  };

  struct PatchContextKeyHash {
    std::size_t operator()(PatchContextKey const& key) const {
      return hash_of(std::addressof(key.source.get()), key.path);
    }
  };

  RefPtr<LuaEngine> m_lua_engine;
  mutable HashMap<PatchContextKey, std::shared_ptr<LuaContext>, PatchContextKeyHash> m_patch_contexts;
  mutable std::recursive_mutex m_lua_mutex;

  // Paths of all used asset sources, in load order.
  StringList m_asset_sources;
  // Built during initialization; values borrow paths from m_asset_sources.
  StringMap<std::reference_wrapper<String const>> m_asset_source_paths_by_name;

  // Maps an asset path to the loaded asset source and vice versa
  BiMap<String, std::shared_ptr<AssetSource>> m_asset_source_paths;

  // Maps the source asset name to the source containing it
  CaseInsensitiveStringMap<AssetFileDescriptor> m_files;
  // Maps an extension to the files with that extension
  CaseInsensitiveStringMap<CaseInsensitiveStringSet> m_files_by_extension;

  ByteArray m_digest;

  // Requests and CV notifications use m_assetsMutex; shared by this worker run.
  std::stop_source m_worker_stop_source{std::nostopstate};
  mutable std::optional<StarException> m_worker_error;
  List<ThreadFunction<void>> m_worker_threads;
};

}

export module star.assets;

export namespace Star {
  using ::Star::Font;
  using ::Star::Audio;
  using ::Star::Image;
  using ::Star::FramesSpecification;
  using ::Star::asset_path_range;
  using ::Star::Assets;
  using ::Star::LuaContext;
  using ::Star::AssetExceptionTag;
  using ::Star::AssetException;
}
