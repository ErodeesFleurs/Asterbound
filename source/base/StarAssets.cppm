module;
import std;
import star.string;
import star.map;
import star.format;
import star.json;
import star.either;
import star.byte_array;
import star.ordered_map;
import star.rect;
import star.bi_map;
import star.thread;
import star.io_device;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ref_ptr;
import star.type_identity;

import star.asset_source;
import star.font;

namespace Star {

class Logger;
class RandomService;
class LogMap;

class Font;
class Audio;
class Image;

class LuaContext;

struct AssetExceptionTag {
  static constexpr std::string_view name() { return "AssetException"; }
};
using AssetException = StarError<AssetExceptionTag, StarException>;

// The contents of an assets .frames file, which can be associated with one or
// more images, and specifies named sub-rects of those images.
struct FramesSpecification {
  // Get the target sub-rect of a given frame name (which can be an alias).
  // Returns nothing if the frame name is not found.
  [[nodiscard]] std::expected<std::optional<RectU>, StarException> getRect(String const& frame) const;
  // Converts to Json.
  [[nodiscard]] std::expected<Json, StarException> toJson() const;
  // The full path to the .frames file from which this was loaded.
  String framesFile;
  // Named sub-frames
  StringMap<RectU> frames;
  // Aliases for named sub-frames, always points to a valid frame name in the
  // 'frames' map.
  StringMap<String> aliases;
};

// The assets system can load image, font, json, and data assets from a set of
// sources.  Each source is either a directory on the filesystem or a single
// packed asset file.
//
// Assets is thread safe and performs TTL caching.
class Assets {
public:
  struct Settings {
    // TTL for cached assets
    float assetTimeToLive;

    // Audio under this length will be automatically decompressed
    float audioDecompressLimit;

    // Number of background worker threads
    unsigned workerPoolSize;

    // If given, if an image is unable to load, will log the error and load
    // this path instead
    std::optional<String> missingImage;

    // Same, but for audio
    std::optional<String> missingAudio;

    // When loading assets from a directory, will automatically ignore any
    // files whose asset paths matching any of the given patterns.
    StringList pathIgnore;

    // Same, but only ignores the file for the purposes of calculating the
    // digest.
    StringList digestIgnore;

    // Longer lifetime for recently referenced blend/mask images. The pixel
    // budget limits this extension, not total cache or externally held memory.
    float referenceImageTimeToLive = 300;
    std::uint64_t referenceImagePixelBudget = 16 * 1024 * 1024;
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

    bool operator==(AssetId const& assetId) const;
  };

  struct AssetIdHash {
    std::size_t operator()(AssetId const& id) const;
  };

  struct AssetData {
    virtual ~AssetData() = default;

    virtual void const* query_type(TypeKey key) const noexcept { return query_type_of<AssetData>(this, key); }

    virtual std::string_view dynamic_type_name() const noexcept { return Star::type_name<AssetData>(); }

    // Should return true if this asset is shared and still in use, so freeing
    // it from cache will not really free the resource, so it should persist in
    // the cache.
    virtual bool shouldPersist() const = 0;

    // Keep absolute seconds at double precision before computing cache ages.
    std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>> time{};
    bool needsPostProcessing = false;
    bool forcePersist = false;
  };

  struct JsonData : AssetData {
    void const* query_type(TypeKey key) const noexcept override {
      return query_type_of<JsonData, AssetData>(this, key);
    }

    std::string_view dynamic_type_name() const noexcept override { return Star::type_name<JsonData>(); }

    bool shouldPersist() const override;

    Json json;
  };

  // Image data for an image, sub-frame, or post-processed image.
  struct ImageData : AssetData {
    void const* query_type(TypeKey key) const noexcept override {
      return query_type_of<ImageData, AssetData>(this, key);
    }

    std::string_view dynamic_type_name() const noexcept override { return Star::type_name<ImageData>(); }

    bool shouldPersist() const override;

    std::shared_ptr<Image const> image;

    // *Optional* sub-frames data for this image, only will exist when the
    // image is a top-level image and has an associated frames file.
    std::shared_ptr<FramesSpecification const> frames;

    // If this image aliases another asset entry, this will be true and
    // shouldPersist will never be true (to ensure that this alias and its
    // target can be removed from the cache).
    bool alias = false;

    bool isReference = false;
  };

  struct AudioData : AssetData {
    void const* query_type(TypeKey key) const noexcept override {
      return query_type_of<AudioData, AssetData>(this, key);
    }

    std::string_view dynamic_type_name() const noexcept override { return Star::type_name<AudioData>(); }

    bool shouldPersist() const override;

    std::shared_ptr<Audio const> audio;
  };

  struct FontData : AssetData {
    void const* query_type(TypeKey key) const noexcept override {
      return query_type_of<FontData, AssetData>(this, key);
    }

    std::string_view dynamic_type_name() const noexcept override { return Star::type_name<FontData>(); }

    bool shouldPersist() const override;

    std::shared_ptr<Font const> font;
  };

  struct BytesData : AssetData {
    void const* query_type(TypeKey key) const noexcept override {
      return query_type_of<BytesData, AssetData>(this, key);
    }

    std::string_view dynamic_type_name() const noexcept override { return Star::type_name<BytesData>(); }

    bool shouldPersist() const override;

    std::shared_ptr<ByteArray const> bytes;
  };

  struct AssetFileDescriptor {
    // The mixed case original source name;
    String sourceName;
    // The source that has the primary asset copy
    std::shared_ptr<AssetSource> source;
    // List of source names and sources for patches to this file.
    List<std::pair<String, std::shared_ptr<AssetSource>>> patchSources;
  };

  [[nodiscard]] static std::expected<std::shared_ptr<Assets>, StarException> create(Logger& logger, RandomService& random, LogMap& logMap, Settings settings, StringList assetSources);
  ~Assets();

  void hotReload() const;

  // Returns a list of all the asset source paths used by Assets in load order.
  StringList assetSources() const;

  // Return metadata for the given loaded asset source path
  [[nodiscard]] std::expected<JsonObject, StarException> assetSourceMetadata(String const& sourcePath) const;

  // An imperfect sha256 digest of the contents of all combined asset sources.
  // Useful for detecting if there are mismatched assets between a client and
  // server or if assets sources have changed from a previous load.
  ByteArray digest() const;

  // Is there an asset associated with the given path?  Path must not contain
  // sub-paths or directives.
  [[nodiscard]] std::expected<bool, StarException> assetExists(String const& path) const;

  [[nodiscard]] std::expected<std::optional<AssetFileDescriptor>, StarException> assetDescriptor(String const& path) const;

  // The name of the asset source within which the path exists.
  [[nodiscard]] std::expected<String, StarException> assetSource(String const& path) const;

  std::optional<String> assetSourcePath(std::shared_ptr<AssetSource> const& source) const;

  // Describe a held provider without resolving a possibly overridden virtual path.
  // Missing or invalid optional metadata falls back to the physical source path.
  static std::string sourceDiagnostic(AssetSource const& source,
      std::optional<std::reference_wrapper<String const>> sourcePath);
  StarException withAssetContext(StarException error, String const& path, std::string_view operation) const;

  // Scans for all assets with the given suffix in any directory.
  [[nodiscard]] std::expected<StringList, StarException> scan(String const& suffix) const;
  // Scans for all assets matching both prefix and suffix (prefix may be, for
  // example, a directory)
  [[nodiscard]] std::expected<StringList, StarException> scan(String const& prefix, String const& suffix) const;
  // Scans all assets for files with the given extension, which is specially
  // indexed and much faster than a normal scan.  Extension may contain leading
  // '.' character or it may be omitted.
  [[nodiscard]] std::expected<std::reference_wrapper<CaseInsensitiveStringSet const>, StarException> scanExtension(String const& extension) const;

  // Get json asset with an optional sub-path.  The sub-path portion of the
  // path refers to a key in the top-level object, and may use dot notation
  // for deeper field access and [] notation for array access.  Example:
  // "/path/to/json:key1.key2.key3[4]".
  [[nodiscard]] std::expected<Json, StarException> json(String const& path) const;

  // Either returns the json v, or, if v is a string type, returns the json
  // pointed to by interpreting v as a string path.
  [[nodiscard]] std::expected<Json, StarException> fetchJson(Json const& v, String const& dir = "/") const;

  // Load all the given jsons using background processing.
  [[nodiscard]] std::expected<void, StarException> queueJsons(StringList const& paths) const;
  [[nodiscard]] std::expected<void, StarException> queueJsons(CaseInsensitiveStringSet const& paths) const;

  // Returns *either* an image asset or a sub-frame.  Frame files are JSON
  // descriptor files that reference a particular image and label separate
  // sub-rects of the image.  If the given path has a ':' sub-path, then the
  // assets system will look for an associated .frames named either
  // <full-path-minus-extension>.frames or default.frames, going up to assets
  // root.  May return the same ImageConstPtr for different paths if the paths
  // are equivalent or they are aliases of other image paths.
  [[nodiscard]] std::expected<std::shared_ptr<Image const>, StarException> image(AssetPath const& path) const;
  // Load images using background processing.
  [[nodiscard]] std::expected<void, StarException> queueImages(StringList const& paths) const;
  [[nodiscard]] std::expected<void, StarException> queueImages(CaseInsensitiveStringSet const& paths) const;
  // A successful null result means the image was queued for loading.
  [[nodiscard]] std::expected<std::shared_ptr<Image const>, StarException> tryImage(AssetPath const& path) const;

  // Returns the best associated FramesSpecification for a given image path, if
  // it exists.  The given path must not contain sub-paths or directives, and
  // this function may return nullptr if no frames file is associated with the
  // given image path.
  [[nodiscard]] std::expected<std::shared_ptr<FramesSpecification const>, StarException> imageFrames(String const& path) const;

  [[nodiscard]] std::expected<std::shared_ptr<Audio const>, StarException> audio(String const& path) const;
  [[nodiscard]] std::expected<void, StarException> queueAudios(StringList const& paths) const;
  [[nodiscard]] std::expected<void, StarException> queueAudios(CaseInsensitiveStringSet const& paths) const;
  // A successful null result means the audio was queued for loading.
  [[nodiscard]] std::expected<std::shared_ptr<Audio const>, StarException> tryAudio(String const& path) const;

  // Returns pointer to shared font asset
  [[nodiscard]] std::expected<std::shared_ptr<Font const>, StarException> font(String const& path) const;

  // Returns a bytes asset (Reads asset as an opaque binary blob)
  [[nodiscard]] std::expected<std::shared_ptr<ByteArray const>, StarException> bytes(String const& path) const;

  // Bypass asset caching and open an asset file directly.
  [[nodiscard]] std::expected<std::shared_ptr<IODevice>, StarException> openFile(String const& basePath) const;

  // Clear all cached assets that are not queued, persistent, or broken.
  void clearCache();

  // Run a cleanup pass and remove any assets past their time to live.
  void cleanup();

private:
  friend struct AssetWorkerBenchmarkAccess;
  friend struct AssetWorkerTestAccess;

  explicit Assets(Logger& logger);
  [[nodiscard]] std::expected<void, StarException> initialize(RandomService& random, LogMap& logMap, Settings settings, StringList assetSources);

  [[nodiscard]] static std::expected<FramesSpecification, StarException> parseFramesSpecification(Json const& frameConfig, String path);

  void queueAssets(List<AssetId> const& assetIds) const;
  //Lock before calling!
  bool queueAsset(AssetId const& assetId) const;
  [[nodiscard]] std::expected<std::shared_ptr<AssetData>, StarException> tryAsset(AssetId const& id) const;
  [[nodiscard]] std::expected<std::shared_ptr<AssetData>, StarException> getAsset(AssetId const& id) const;

  [[nodiscard]] std::expected<void, StarException> workerMain(std::stop_token stopToken);
  [[nodiscard]] std::expected<void, StarException> stopWorkers();

  // All methods below assume that the asset mutex is locked when calling.

  // Do some processing that might take a long time and should not hold the
  // assets mutex during it. Unlocks for the callback and re-locks before return.
  template <typename Function>
  [[nodiscard]] decltype(auto) unlockDuring(Function f) const;

  // Returns the best frames specification for the given image path, if it exists.
  [[nodiscard]] std::expected<std::shared_ptr<FramesSpecification const>, StarException> bestFramesSpecification(String const& basePath) const;

  [[nodiscard]] std::expected<std::shared_ptr<IODevice>, StarException> open(String const& basePath) const;
  [[nodiscard]] std::expected<ByteArray, StarException> read(String const& basePath) const;
  [[nodiscard]] std::expected<std::shared_ptr<Image const>, StarException> readImage(String const& path) const;
  [[nodiscard]] std::expected<std::shared_ptr<Image const>, StarException> applyImagePatches(std::shared_ptr<Image const> image, String const& path, List<std::pair<String, std::shared_ptr<AssetSource>>> patches) const;

  [[nodiscard]] std::expected<Json, StarException> readJson(String const& basePath) const;
  [[nodiscard]] std::expected<Json, StarException> applyJsonPatches(Json const& input, String const& path, List<std::pair<String, std::shared_ptr<AssetSource>>> patches) const;
  [[nodiscard]] std::expected<void, StarException> reportPatchRejection(String const& path,
      std::shared_ptr<AssetSource> const& source, StarException const& error) const;
  [[nodiscard]] std::expected<Json, StarException> checkPatchArray(String const& path,
                                                                   std::shared_ptr<AssetSource> const& source,
                                                                   Json const result, JsonArray const& patchData,
                                                                   std::optional<Json> const external) const;

  // Load / post process an asset and log any failure. Returns true if the
  // work was performed (whether successful or not), false if the work is
  // blocking on something.
  [[nodiscard]] std::expected<bool, StarException> doLoad(AssetId const& id) const;
  [[nodiscard]] std::expected<bool, StarException> doPost(AssetId const& id) const;

  // A successful null result means loading is blocked by another busy asset.
  [[nodiscard]] std::expected<std::shared_ptr<AssetData>, StarException> loadAsset(AssetId const& id) const;
  void publishAssetFailure(AssetId const& id, StarException const& error) const;

  [[nodiscard]] std::expected<std::shared_ptr<AssetData>, StarException> loadJson(AssetPath const& path) const;
  [[nodiscard]] std::expected<std::shared_ptr<AssetData>, StarException> loadImage(AssetPath const& path) const;
  [[nodiscard]] std::expected<std::shared_ptr<AssetData>, StarException> loadAudio(AssetPath const& path) const;
  [[nodiscard]] std::expected<std::shared_ptr<AssetData>, StarException> loadFont(AssetPath const& path) const;
  [[nodiscard]] std::expected<std::shared_ptr<AssetData>, StarException> loadBytes(AssetPath const& path) const;

  [[nodiscard]] std::expected<std::shared_ptr<AssetData>, StarException> postProcessAudio(std::shared_ptr<AssetData> const& original) const;

  // Updates the asset's monotonic access time.
  void freshen(std::shared_ptr<AssetData> const& asset) const;

  Logger& m_logger;
  std::optional<FontLibrary> m_fontLibrary;

  Settings m_settings;

  mutable std::mutex m_assetsMutex;

  mutable std::condition_variable m_assetsQueued;
  mutable OrderedHashMap<AssetId, QueuePriority, AssetIdHash> m_queue;
  // Protected by m_assetsMutex. One worker claims collection for a drained
  // load burst; in-flight work must not compete with idle full collections.
  mutable bool m_luaGcPending = true;

  mutable std::condition_variable m_assetsDone;
  mutable HashMap<AssetId, std::shared_ptr<AssetData>, AssetIdHash> m_assetsCache;
  mutable HashMap<AssetId, StarException, AssetIdHash> m_assetErrors;

  mutable StringMap<String> m_bestFramesFiles;
  mutable StringMap<std::shared_ptr<FramesSpecification const>> m_framesSpecifications;

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

  RefPtr<RefCounter> m_luaEngine; // dumb but to avoid including Lua.hpp in here...
  mutable HashMap<PatchContextKey, std::shared_ptr<LuaContext>, PatchContextKeyHash> m_patchContexts;
  mutable std::recursive_mutex m_luaMutex;

  // Paths of all used asset sources, in load order.
  StringList m_assetSources;

  // Maps an asset path to the loaded asset source and vice versa
  BiMap<String, std::shared_ptr<AssetSource>> m_assetSourcePaths;

  // Maps the source asset name to the source containing it
  CaseInsensitiveStringMap<AssetFileDescriptor> m_files;
  // Maps an extension to the files with that extension
  CaseInsensitiveStringMap<CaseInsensitiveStringSet> m_filesByExtension;

  ByteArray m_digest;

  // Requests and CV notifications use m_assetsMutex; shared by this worker run.
  std::stop_source m_workerStopSource{std::nostopstate};
  mutable std::optional<StarException> m_workerError;
  List<ThreadFunction<void>> m_workerThreads;
};

}

export module star.assets;

export namespace Star {
  using ::Star::Font;
  using ::Star::Audio;
  using ::Star::Image;
  using ::Star::FramesSpecification;
  using ::Star::Assets;
  using ::Star::LuaContext;
  using ::Star::AssetExceptionTag;
  using ::Star::AssetException;
}
