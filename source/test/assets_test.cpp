import star.json;
import star.json_extra;
import star.ordered_map;
import star.bi_map;
import star.thread;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ref_ptr;
import star.io_device;
import star.string;
import star.file;
import star.format;
import std;

import star.assets;
import star.asset_source;
import star.directory_asset_source;
import star.memory_asset_source;
import star.packed_asset_source;
import star.data_stream_devices;
import star.data_stream_extra;
import star.sha256;
import star.animated_part_set;
import star.audio;
import star.buffer;
import star.image;
import star.byte_array;
import star.vector;
import star.game_test;
import star.root;
import star.random;
import star.logging;
import star.effect_source_database;
import star.lua;
import star.lua_game_converters;
import star.mixer;
import star.dance_database;
import star.object_database;
import star.world_server;
import star.game_types;
import star.drawable;
import star.rect;

#include "gtest/gtest.h"

import star.algorithm;
namespace Star {
struct AssetWorkerTestAccess {
  static std::expected<void, StarException> drain(Assets& assets) {
    std::unique_lock lock(assets.m_assetsMutex);
    if (!assets.m_assetsDone.wait_for(lock, std::chrono::seconds(60), [&] {
          return assets.m_queue.empty() || assets.m_workerError || assets.m_workerStopSource.stop_requested();
        }))
      return std::unexpected(StarException("Asset queue regression did not drain"));
    if (assets.m_workerError)
      return std::unexpected(*assets.m_workerError);
    if (assets.m_workerStopSource.stop_requested())
      return std::unexpected(StarException("Asset queue stopped before draining"));
    return {};
  }

  // Retain the former reader-based algorithm as an independent byte oracle.
  static std::expected<ByteArray, StarException> reader_digest(Assets& assets) {
    Sha256Hasher digest;
    StringList paths;
    for (auto const& entry : assets.m_files) {
      auto lower = entry.first.string().to_lower();
      if (!lower)
        return std::unexpected(std::move(lower.error()));
      paths.push_back(std::move(*lower));
    }
    paths.sort();
    for (auto const& path : paths) {
      bool ignored = false;
      for (auto const& pattern : assets.m_settings.digestIgnore) {
        auto matches = path.regex_match(pattern, false, false);
        if (!matches)
          return std::unexpected(std::move(matches.error()));
        if (*matches) {
          ignored = true;
          break;
        }
      }
      if (ignored)
        continue;
      auto key = CaseInsensitiveStringView::create(path);
      if (!key)
        return std::unexpected(std::move(key.error()));
      auto descriptor = assets.m_files.get(*key);
      if (!descriptor)
        return std::unexpected(std::move(descriptor.error()));
      digest.push(path);
      auto device = descriptor->get().source->open(descriptor->get().sourceName);
      if (!device)
        return std::unexpected(std::move(device.error()));
      auto size = (*device)->size();
      if (!size)
        return std::unexpected(std::move(size.error()));
      auto bytes = DataStreamBuffer::serialize(*size);
      if (!bytes)
        return std::unexpected(std::move(bytes.error()));
      digest.push(*bytes);
      for (auto const& patch : descriptor->get().patchSources) {
        auto patch_device = patch.second->open(AssetPath::removeSubPath(patch.first));
        if (!patch_device)
          return std::unexpected(std::move(patch_device.error()));
        auto patch_size = (*patch_device)->size();
        if (!patch_size)
          return std::unexpected(std::move(patch_size.error()));
        auto patch_bytes = DataStreamBuffer::serialize(*patch_size);
        if (!patch_bytes)
          return std::unexpected(std::move(patch_bytes.error()));
        digest.push(*patch_bytes);
      }
    }
    return digest.compute();
  }
};
}

using namespace Star;

namespace {
  std::filesystem::path fsPath(String const& path) {
    return std::filesystem::path(path.utf8());
  }

  void expect_batch_matches_scalar(AssetSource& source, std::span<String const> paths) {
    std::vector<std::reference_wrapper<String const>> requests;
    requests.reserve(paths.size());
    for (auto const& path : paths)
      requests.emplace_back(path);
    auto batch = source.file_sizes(requests);
    ASSERT_EQ(batch.size(), paths.size());
    for (std::size_t i = 0; i != paths.size(); ++i) {
      SCOPED_TRACE(paths[i].utf8());
      auto scalar = source.file_size(paths[i]);
      ASSERT_EQ(batch[i].has_value(), scalar.has_value());
      if (scalar)
        EXPECT_EQ(*batch[i], *scalar);
      else
        EXPECT_STREQ(batch[i].error().what(), scalar.error().what());
    }
    EXPECT_TRUE(source.file_sizes({}).empty());
  }

  struct PngIoFailureTag {
    static constexpr std::string_view name() { return "PngIoFailure"; }
  };
  using PngIoFailure = StarError<PngIoFailureTag, StarException>;

  class FailingPngNameBuffer : public Buffer {
  public:
    explicit FailingPngNameBuffer(ByteArray bytes) : Buffer(std::move(bytes)) {}
    [[nodiscard]] std::expected<String, StarException> device_name() const override {
      return std::unexpected(PngIoFailure("Injected PNG device-name failure"));
    }
  };

  class FailingPngBuffer : public Buffer {
  public:
    FailingPngBuffer(ByteArray bytes, StreamOffset failAt) : Buffer(std::move(bytes)), m_failAt(failAt) {}

    [[nodiscard]] std::expected<void, StarException> read_full(std::span<char> data) override {
      auto ready = failIfRequested();
      if (!ready)
        return std::unexpected(std::move(ready.error()));
      return Buffer::read_full(data);
    }

    [[nodiscard]] std::expected<void, StarException> write_full(std::span<char const> data) override {
      auto ready = failIfRequested();
      if (!ready)
        return std::unexpected(std::move(ready.error()));
      return Buffer::write_full(data);
    }

    unsigned unwound = 0;

  private:
    [[nodiscard]] std::expected<void, StarException> failIfRequested() {
      auto position = pos();
      if (!position)
        return std::unexpected(std::move(position.error()));
      if (*position >= m_failAt) {
        struct CleanupWitness {
          unsigned& count;
          ~CleanupWitness() { ++count; }
        } witness{unwound};
        return std::unexpected(PngIoFailure("Injected PNG transfer failure"));
      }
      return {};
    }

    StreamOffset m_failAt;
  };

  [[nodiscard]] std::expected<Image, StarException> pngTestImage(PixelFormat format) {
    Image image(2, 2, format);
    for (auto const& [position, color] : std::array{
        std::pair{Vec2U{0, 0}, Vec4B{255, 0, 0, 64}},
        std::pair{Vec2U{1, 0}, Vec4B{0, 255, 0, 128}},
        std::pair{Vec2U{0, 1}, Vec4B{0, 0, 255, 192}},
        std::pair{Vec2U{1, 1}, Vec4B{255, 255, 0, 255}}}) {
      auto set = image.set(position[0], position[1], color);
      if (!set)
        return std::unexpected(std::move(set.error()));
    }
    return image;
  }

  [[nodiscard]] std::expected<ByteArray, StarException> pngTestBytes(Logger& logger) {
    auto buffer = std::make_shared<Buffer>();
    auto image = pngTestImage(PixelFormat::RGBA32);
    if (!image)
      return std::unexpected(std::move(image.error()));
    auto written = image->writePng(logger, buffer);
    if (!written)
      return std::unexpected(std::move(written.error()));
    return buffer->takeData();
  }
}

const GameTestRegistration assetsAll("AssetsTest", "All", [](GameTest&) {
  EXPECT_EQ(AssetPath::removeDirectives("/foo/bar/baz??::?:??:asdfasdf??D?"), "/foo/bar/baz");
  EXPECT_EQ(AssetPath::directory("/foo/bar/baz"), "/foo/bar/");
  EXPECT_EQ(AssetPath::directory("foo/bar/baz"), "foo/bar/");
  EXPECT_EQ(AssetPath::directory("foo"), "");
  EXPECT_EQ(AssetPath::directory("/foo"), "/");
  EXPECT_EQ(AssetPath::filename(""), "");
  EXPECT_EQ(AssetPath::filename("foo"), "foo");
  EXPECT_EQ(AssetPath::filename("/foo"), "foo");
  EXPECT_EQ(AssetPath::filename("/foo/"), "");
  EXPECT_EQ(AssetPath::filename("/foo/bar"), "bar");

  EXPECT_EQ(AssetPath::relative_to("/foo/bar/baz:baf?whoa?there", "thing:sub?directive"),
            "/foo/bar/thing:sub?directive");
});

const GameTestRegistration objectCursorPreviewRandomIsolation(
  "AssetsTest", "ObjectCursorPreviewColdAndWarmCachesPreserveGameplayRandom", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto settings = fixture.rootSettings();
    settings.storageDirectory = File::relative_to(*directory, "storage");
    settings.runtimeConfigFile = {};
    settings.logDirectory = {};
    settings.logFile = {};
    settings.includeUGC = false;
    auto isolatedRoot = Root::create(std::move(settings));
    ASSERT_TRUE(isolatedRoot) << isolatedRoot.error().what();
    auto& root = **isolatedRoot;
    auto assets = root.assets();
    ASSERT_TRUE(assets) << assets.error().what();
    auto files = (*assets)->scanExtension("object");
    ASSERT_TRUE(files) << files.error().what();
    String objectName;
    for (auto const& path : files->get()) {
      auto config = (*assets)->json(path.string());
      ASSERT_TRUE(config) << config.error().what();
      auto flickering = config->contains("flickerPeriod");
      ASSERT_TRUE(flickering);
      if (*flickering) {
        auto name = config->getString("objectName");
        ASSERT_TRUE(name);
        objectName = std::move(*name);
        break;
      }
    }
    ASSERT_FALSE(objectName.empty()) << "The installed assets must supply a flickering object for this regression";
    SCOPED_TRACE(objectName.utf8());

    auto storage = File::ephemeral_file(root.random());
    ASSERT_TRUE(storage) << storage.error().what();
    auto world = WorldServer::create(root, Vec2U(2048, 2048), *storage);
    ASSERT_TRUE(world) << world.error().what();
    auto closeWorld = finally([&] { EXPECT_TRUE((*world)->close()); });
    (*world)->setSpawningEnabled(false);
    Vec2I const position(1024, 1024);
    ASSERT_TRUE((*world)->generateRegion(RectI::withSize(position - Vec2I(64, 64), Vec2I(128, 128))));

    // There is no UniverseServer or world thread. Finish world/database setup
    // before seeding, and use a fresh database so the first preview really
    // parses the installed config, including its randomized light flicker.
    for (auto direction : {Direction::Left, Direction::Right}) {
      auto database = ObjectDatabase::create(root);
      ASSERT_TRUE(database) << database.error().what();
      constexpr std::uint64_t seed = 0x435552534f525052;
      root.random().init(seed);
      RandomSource untouchedGameplay(seed);
      auto cold = (*database)->cursorHintDrawables(**world, objectName, position, direction, JsonObject{});
      ASSERT_TRUE(cold) << cold.error().what();
      ASSERT_FALSE(cold->empty());
      ASSERT_EQ(root.random().randu64(), untouchedGameplay.randu64()) << "cold preview";
      auto cachedConfig = (*database)->getConfig(objectName);
      ASSERT_TRUE(cachedConfig) << cachedConfig.error().what();
      ASSERT_TRUE((*cachedConfig)->lightFlickering);
      ASSERT_EQ(root.random().randu64(), untouchedGameplay.randu64()) << "warm config lookup";
      auto warm = (*database)->cursorHintDrawables(**world, objectName, position, direction, JsonObject{});
      ASSERT_TRUE(warm) << warm.error().what();
      EXPECT_EQ(*warm, *cold);
      ASSERT_EQ(root.random().randu64(), untouchedGameplay.randu64()) << "warm preview";
    }
  });

const GameTestRegistration assetsSymlinkLoopIsNotFollowed("AssetsTest", "SymlinkLoopIsNotFollowed", [](GameTest& fixture) {
  // A link back to an ancestor must not make asset scanning fail with ELOOP.
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory);
  auto tempDir = std::move(*directory);
  auto finallyGuard = finally([&tempDir]() { EXPECT_TRUE(File::remove_all(tempDir)); });

  std::error_code ec;
  std::filesystem::create_directories(fsPath(File::relative_to(tempDir, "real")), ec);
  ASSERT_FALSE(ec) << ec.message();
  ASSERT_TRUE(File::write_file(String("data"), File::relative_to(File::relative_to(tempDir, "real"), "asset.txt")));
  std::filesystem::create_directory_symlink(fsPath(File::relative_to(tempDir, "real")),
                                            fsPath(File::relative_to(tempDir, "link")), ec);
  if (ec)
    GTEST_SKIP() << "cannot create directory symlinks here: " << ec.message();
  std::filesystem::create_directory_symlink(fsPath(tempDir), fsPath(File::relative_to(tempDir, "loop")), ec);
  if (ec)
    GTEST_SKIP() << "cannot create directory symlinks here: " << ec.message();

  auto source = DirectoryAssetSource::create(fixture.root().logger(), tempDir);
  ASSERT_TRUE(source);
  auto paths = (*source)->assetPaths();
  auto real = paths.contains("/real/asset.txt");
  ASSERT_TRUE(real);
  EXPECT_TRUE(*real);
  auto sibling = paths.contains("/link/asset.txt");
  ASSERT_TRUE(sibling);
  EXPECT_TRUE(*sibling);
  auto loopReal = paths.contains("/loop/real/asset.txt");
  ASSERT_TRUE(loopReal);
  EXPECT_FALSE(*loopReal);
  auto loopSibling = paths.contains("/loop/link/asset.txt");
  ASSERT_TRUE(loopSibling);
  EXPECT_FALSE(*loopSibling);
});

const GameTestRegistration
  assetMetadataLengths("AssetsTest", "MetadataLengthsStayFreshAndRejectMissingOrReplacedFiles", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto path = File::relative_to(*directory, "mutable.json");
    ASSERT_TRUE(File::write_file(String("{}"), path));
    ASSERT_TRUE(File::write_file(String("[]"), File::relative_to(*directory, "preload.config")));
    auto source = DirectoryAssetSource::create(fixture.root().logger(), *directory);
    ASSERT_TRUE(source);
    auto size = (*source)->file_size("/mutable.json");
    ASSERT_TRUE(size);
    EXPECT_EQ(*size, 2);
    EXPECT_FALSE((*source)->file_size("/MUTABLE.json"));
    EXPECT_FALSE((*source)->file_size("/../mutable.json"));
    EXPECT_FALSE((*source)->file_size("/"));
    auto added = File::relative_to(*directory, "unindexed.txt");
    ASSERT_TRUE(File::write_file(String("not enumerated"), added));
    EXPECT_FALSE((*source)->file_size("/unindexed.txt"));
    std::array<String, 7> requests{"/mutable.json",    "/missing", "/mutable.json", "/MUTABLE.json",
                                   "/../mutable.json", "/",        "/unindexed.txt"};
    expect_batch_matches_scalar(**source, requests);

    Assets::Settings settings{};
    settings.workerPoolSize = 1;
    auto assets =
      Assets::create(fixture.root().logger(), fixture.root().random(), fixture.root().logMap(), settings, {*directory});
    ASSERT_TRUE(assets) << assets.error().what();
    ASSERT_TRUE((*assets)->json("/mutable.json"));
    String replacement(R"({"recovered":42})");
    ASSERT_TRUE(File::write_file(replacement, path));
    (*assets)->hotReload();
    size = (*source)->file_size("/mutable.json");
    ASSERT_TRUE(size);
    EXPECT_EQ(*size, static_cast<StreamOffset>(replacement.utf8_size()));
    expect_batch_matches_scalar(**source, requests);
    auto recovered = (*assets)->json("/mutable.json");
    ASSERT_TRUE(recovered);
    EXPECT_EQ(recovered->getInt("recovered"), 42);

    ASSERT_TRUE(File::remove(path));
    auto missing = (*source)->file_size("/mutable.json");
    ASSERT_FALSE(missing);
    EXPECT_TRUE(missing.error().is<IOException>());
    expect_batch_matches_scalar(**source, requests);
    std::error_code ec;
    ASSERT_TRUE(std::filesystem::create_directory(fsPath(path), ec));
    ASSERT_FALSE(ec) << ec.message();
    auto replaced = (*source)->file_size("/mutable.json");
    ASSERT_FALSE(replaced);
    EXPECT_TRUE(replaced.error().is<IOException>());
    EXPECT_FALSE((*source)->open("/mutable.json"));
    expect_batch_matches_scalar(**source, requests);
    ASSERT_TRUE(std::filesystem::remove(fsPath(path), ec));
    ASSERT_FALSE(ec) << ec.message();
    ASSERT_TRUE(File::write_file(String("{}"), path));
    size = (*source)->file_size("/mutable.json");
    ASSERT_TRUE(size);
    EXPECT_EQ(*size, 2);
    expect_batch_matches_scalar(**source, requests);
    (*assets)->hotReload();
    EXPECT_TRUE((*assets)->json("/mutable.json"));

    MemoryAssetSource memory("metadata-test");
    memory.set("/bytes", ByteArray(std::span("one\0two", 7)));
    memory.set("/image", Image(3, 2, PixelFormat::RGBA32));
    auto bytes_size = memory.file_size("/bytes");
    ASSERT_TRUE(bytes_size);
    EXPECT_EQ(*bytes_size, 7);
    auto image_size = memory.file_size("/image");
    ASSERT_TRUE(image_size);
    EXPECT_EQ(*image_size, 24);
    std::array<String, 4> memory_requests{"/bytes", "/missing", "/image", "/bytes"};
    expect_batch_matches_scalar(memory, memory_requests);
    for (auto const& asset_path : memory.assetPaths()) {
      auto reader = memory.open(asset_path);
      ASSERT_TRUE(reader);
      auto reader_size = (*reader)->size();
      ASSERT_TRUE(reader_size);
      auto metadata_size = memory.file_size(asset_path);
      ASSERT_TRUE(metadata_size);
      EXPECT_EQ(*metadata_size, *reader_size);
    }
    memory.set("/bytes", ByteArray{});
    bytes_size = memory.file_size("/bytes");
    ASSERT_TRUE(bytes_size);
    EXPECT_EQ(*bytes_size, 0);
    expect_batch_matches_scalar(memory, memory_requests);
    ASSERT_TRUE(memory.erase("/bytes"));
    EXPECT_FALSE(memory.file_size("/bytes"));
    expect_batch_matches_scalar(memory, memory_requests);
  });

const GameTestRegistration packedMetadataLengths(
  "AssetsTest", "PackedMetadataLengthsValidateFreshExtentAndMalformedEntries", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto original = File::relative_to(*directory, "original");
    std::error_code ec;
    ASSERT_TRUE(std::filesystem::create_directory(fsPath(original), ec));
    ASSERT_FALSE(ec) << ec.message();
    ASSERT_TRUE(File::write_file(String(std::string_view("one\0two", 7)), File::relative_to(original, "binary")));
    auto source = DirectoryAssetSource::create(fixture.root().logger(), original);
    ASSERT_TRUE(source);
    auto packed_path = File::relative_to(*directory, "assets.pak");
    ASSERT_TRUE(PackedAssetSource::build(**source, packed_path));
    auto original_bytes = File::read_file(packed_path);
    ASSERT_TRUE(original_bytes);
    auto packed = PackedAssetSource::create(packed_path);
    ASSERT_TRUE(packed);
    auto size = (*packed)->file_size("/binary");
    ASSERT_TRUE(size);
    EXPECT_EQ(*size, 7);
    EXPECT_FALSE((*packed)->file_size("/missing"));
    std::array<String, 4> requests{"/binary", "/missing", "/binary", "/BINARY"};
    expect_batch_matches_scalar(**packed, requests);
    std::array<String, 2> duplicate_paths{"/binary", "/binary"};
    std::array<std::reference_wrapper<String const>, 2> duplicate_requests{std::cref(duplicate_paths[0]),
                                                                           std::cref(duplicate_paths[1])};
    auto before_truncation = (*packed)->file_sizes(duplicate_requests);
    ASSERT_EQ(before_truncation.size(), 2);
    ASSERT_TRUE(before_truncation[0]);
    ASSERT_TRUE(before_truncation[1]);
    auto payload = (*packed)->read("/binary");
    ASSERT_TRUE(payload);
    EXPECT_EQ(*payload, ByteArray(std::span("one\0two", 7)));
    auto writable = File::open(packed_path, IOMode::ReadWrite);
    ASSERT_TRUE(writable);
    ASSERT_TRUE((*writable)->resize(22));// Asset starts at 16 and needs seven bytes.
    auto truncated = (*packed)->file_size("/binary");
    ASSERT_FALSE(truncated);
    EXPECT_TRUE(truncated.error().is<AssetSourceException>());
    expect_batch_matches_scalar(**packed, requests);
    auto during_truncation = (*packed)->file_sizes(duplicate_requests);
    ASSERT_EQ(during_truncation.size(), 2);
    EXPECT_FALSE(during_truncation[0]);
    EXPECT_FALSE(during_truncation[1]);
    // A snapshot is the captured extent of one call, not a cross-call cache
    // and not a promise that the corresponding payload remains immutable.
    EXPECT_EQ(*before_truncation[0], 7);
    EXPECT_EQ(*before_truncation[1], 7);
    EXPECT_FALSE((*packed)->open("/binary"));
    EXPECT_FALSE((*packed)->read("/binary"));
    EXPECT_FALSE(PackedAssetSource::create(packed_path));
    ASSERT_TRUE((*writable)->close());
    ASSERT_TRUE(File::write_file(*original_bytes, packed_path));
    size = (*packed)->file_size("/binary");
    ASSERT_TRUE(size);
    EXPECT_EQ(*size, 7);
    EXPECT_TRUE((*packed)->read("/binary"));
    expect_batch_matches_scalar(**packed, requests);
    auto after_recovery = (*packed)->file_sizes(duplicate_requests);
    ASSERT_EQ(after_recovery.size(), 2);
    ASSERT_TRUE(after_recovery[0]);
    ASSERT_TRUE(after_recovery[1]);
    EXPECT_EQ(*after_recovery[0], 7);
    EXPECT_EQ(*after_recovery[1], 7);

    for (auto const& entry :
         std::array{std::pair<std::uint64_t, std::uint64_t>{std::numeric_limits<std::uint64_t>::max(), 1},
                    std::pair<std::uint64_t, std::uint64_t>{16, std::numeric_limits<std::uint64_t>::max()}}) {
      auto malformed_path = File::relative_to(*directory, "malformed.pak");
      DataStreamBuffer malformed;
      ASSERT_TRUE(malformed.writeData(std::span("SBAsset6", 8)));
      ASSERT_TRUE(malformed.write(std::uint64_t(16)));
      ASSERT_TRUE(malformed.writeData(std::span("INDEX", 5)));
      ASSERT_TRUE(malformed.write(JsonObject{}));
      ASSERT_TRUE(malformed.write(StringMap<std::pair<std::uint64_t, std::uint64_t>>{{"/bad", entry}}));
      ASSERT_TRUE(File::write_file(malformed.data(), malformed_path));
      auto invalid = PackedAssetSource::create(malformed_path);
      ASSERT_TRUE(invalid);
      auto invalid_size = (*invalid)->file_size("/bad");
      ASSERT_FALSE(invalid_size);
      EXPECT_TRUE(invalid_size.error().is<AssetSourceException>());
      std::array<String, 3> invalid_requests{"/missing", "/bad", "/bad"};
      expect_batch_matches_scalar(**invalid, invalid_requests);
      EXPECT_FALSE((*invalid)->open("/bad"));
      EXPECT_FALSE((*invalid)->read("/bad"));
    }
  });

const GameTestRegistration assetMetadataDigest(
  "AssetsTest", "MetadataDigestMatchesFormerReaderBytesForDirectoryAndPackedSources", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto original = File::relative_to(*directory, "original");
    std::error_code ec;
    ASSERT_TRUE(std::filesystem::create_directory(fsPath(original), ec));
    ASSERT_FALSE(ec) << ec.message();
    auto patch_directory = File::relative_to(*directory, "patches");
    ASSERT_TRUE(std::filesystem::create_directory(fsPath(patch_directory), ec));
    ASSERT_FALSE(ec) << ec.message();
    for (auto const& [name, bytes] :
         std::array{std::pair{"preload.config", "[]"}, std::pair{"Mixed.json", R"({"original":true})"},
                    std::pair{"ignored.txt", "excluded"},
                    std::pair{"generated.lua", "assets.add('/memory.json', {generated = true})"},
                    std::pair{"_metadata", R"({"scripts":{"postLoad":["/generated.lua"]}})"}}) {
      ASSERT_TRUE(
        File::write_file(String(std::string_view(bytes)), File::relative_to(original, String(std::string_view(name)))));
    }
    ASSERT_TRUE(File::write_file(String(R"([{"op":"add","path":"/patched","value":true}])"),
                                 File::relative_to(patch_directory, "Mixed.json.patch")));
    auto source = DirectoryAssetSource::create(fixture.root().logger(), original);
    ASSERT_TRUE(source);
    auto packed_path = File::relative_to(*directory, "assets.pak");
    ASSERT_TRUE(PackedAssetSource::build(**source, packed_path));
    auto patch_source = DirectoryAssetSource::create(fixture.root().logger(), patch_directory);
    ASSERT_TRUE(patch_source);
    auto packed_patch_path = File::relative_to(*directory, "patches.pak");
    ASSERT_TRUE(PackedAssetSource::build(**patch_source, packed_patch_path));
    std::optional<ByteArray> directory_digest;
    // Explicit provider order, independent of directory versus packed index traversal.
    for (auto const& [source_path, patch_path] :
         std::array{std::pair{original, patch_directory}, std::pair{packed_path, packed_patch_path}}) {
      Assets::Settings settings{};
      settings.workerPoolSize = 1;
      settings.digestIgnore = {"^/ignored\\.txt$"};
      auto assets = Assets::create(fixture.root().logger(), fixture.root().random(), fixture.root().logMap(), settings,
                                   {source_path, patch_path});
      ASSERT_TRUE(assets) << assets.error().what();
      auto historical = AssetWorkerTestAccess::reader_digest(**assets);
      ASSERT_TRUE(historical) << historical.error().what();
      EXPECT_EQ((*assets)->digest(), *historical);
      if (directory_digest)
        EXPECT_EQ((*assets)->digest(), *directory_digest);
      else
        directory_digest = (*assets)->digest();
      auto generated = (*assets)->json("/memory.json");
      ASSERT_TRUE(generated);
      EXPECT_EQ(*generated, Json(JsonObject{{"generated", true}}));
      auto patched = (*assets)->json("/mixed.json");
      ASSERT_TRUE(patched);
      EXPECT_EQ(*patched, Json(JsonObject{{"original", true}, {"patched", true}}));
    }
  });

const GameTestRegistration assetBatchDigestPatchProviders(
  "AssetsTest", "BatchDigestMatchesReaderOracleWithInlineAndMemoryPatchProviders", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto original = File::relative_to(*directory, "original");
    auto patches = File::relative_to(*directory, "patches");
    ASSERT_TRUE(File::create_directory(original));
    ASSERT_TRUE(File::create_directory(patches));
    ASSERT_TRUE(File::write_file(String("[]"), File::relative_to(original, "preload.config")));
    ASSERT_TRUE(File::write_file(String(R"({"original":true})"), File::relative_to(original, "target.json")));
    ASSERT_TRUE(File::write_file(String(R"({"scripts":{"postLoad":["/generated.lua"]}})"),
                                 File::relative_to(original, "_metadata")));
    ASSERT_TRUE(File::write_file(String(R"lua(
assets.add('/memory.json', {generated = true})
assets.add('/target.json.patch', '[{"op":"add","path":"/memory","value":9}]')
)lua"),
                                 File::relative_to(original, "generated.lua")));
    ASSERT_TRUE(
      File::write_file(String(R"([{"paths":["/target.json"],"patches":[[{"op":"add","path":"/inline","value":7}]]}])"),
                       File::relative_to(patches, "inline.patchlist")));
    auto original_source = DirectoryAssetSource::create(fixture.root().logger(), original);
    auto patch_source = DirectoryAssetSource::create(fixture.root().logger(), patches);
    ASSERT_TRUE(original_source);
    ASSERT_TRUE(patch_source);
    auto packed = File::relative_to(*directory, "original.pak");
    auto packed_patches = File::relative_to(*directory, "patches.pak");
    ASSERT_TRUE(PackedAssetSource::build(**original_source, packed));
    ASSERT_TRUE(PackedAssetSource::build(**patch_source, packed_patches));
    std::optional<ByteArray> directory_digest;
    for (auto const& [source_path, patch_path] :
         std::array{std::pair{original, patches}, std::pair{packed, packed_patches}}) {
      Assets::Settings settings{};
      settings.workerPoolSize = 1;
      auto assets = Assets::create(fixture.root().logger(), fixture.root().random(), fixture.root().logMap(), settings,
                                   {source_path, patch_path});
      ASSERT_TRUE(assets) << assets.error().what();
      auto oracle = AssetWorkerTestAccess::reader_digest(**assets);
      ASSERT_TRUE(oracle) << oracle.error().what();
      EXPECT_EQ((*assets)->digest(), *oracle);
      if (directory_digest)
        EXPECT_EQ((*assets)->digest(), *directory_digest);
      else
        directory_digest = (*assets)->digest();
      auto target = (*assets)->json("/target.json");
      ASSERT_TRUE(target) << target.error().what();
      EXPECT_EQ(*target, Json(JsonObject{{"original", true}, {"inline", 7}, {"memory", 9}}));
      auto memory = (*assets)->json("/memory.json");
      ASSERT_TRUE(memory);
      EXPECT_EQ(*memory, Json(JsonObject{{"generated", true}}));
    }
  });

const GameTestRegistration assetBatchDigestErrorOrder(
  "AssetsTest", "BatchDigestRetainsGlobalFirstErrorAcrossInterleavedProvidersAndPatches", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    using Index = StringMap<std::pair<std::uint64_t, std::uint64_t>>;
    auto write_packed = [](String const& path, Index const& index) -> std::expected<void, StarException> {
      DataStreamBuffer bytes;
      if (auto result = bytes.writeData(std::span("SBAsset6", 8)); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = bytes.write(std::uint64_t(18)); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = bytes.writeData(std::span("[]", 2)); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = bytes.writeData(std::span("INDEX", 5)); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = bytes.write(JsonObject{}); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = bytes.write(index); !result)
        return std::unexpected(std::move(result.error()));
      return File::write_file(bytes.data(), path);
    };
    auto valid = std::pair<std::uint64_t, std::uint64_t>{16, 2};
    auto invalid = std::pair<std::uint64_t, std::uint64_t>{16, std::numeric_limits<std::uint64_t>::max()};
    for (unsigned scenario = 0; scenario != 3; ++scenario) {
      SCOPED_TRACE(scenario);
      auto first_path = File::relative_to(*directory, "first.pak");
      auto second_path = File::relative_to(*directory, "second.pak");
      Index first{{"/a.json", scenario == 1 ? invalid : valid}, {"/preload.config", valid}, {"/z.json", invalid}};
      Index second{{"/b.json", invalid}};
      if (scenario != 2)
        second["/a.json.patch"] = invalid;
      ASSERT_TRUE(write_packed(first_path, first));
      ASSERT_TRUE(write_packed(second_path, second));
      auto first_source = PackedAssetSource::create(first_path);
      auto second_source = PackedAssetSource::create(second_path);
      ASSERT_TRUE(first_source);
      ASSERT_TRUE(second_source);
      // Source A is visited first and contains a later failure (/z.json).
      // The first global failure is instead A's base, B's patch, or B's base.
      auto expected_error = scenario == 1
        ? (*first_source)->file_size("/a.json")
        : (*second_source)->file_size(scenario == 0 ? String("/a.json.patch") : String("/b.json"));
      ASSERT_FALSE(expected_error);
      Assets::Settings settings{};
      settings.workerPoolSize = 1;
      auto assets = Assets::create(fixture.root().logger(), fixture.root().random(), fixture.root().logMap(), settings,
                                   {first_path, second_path});
      ASSERT_FALSE(assets);
      EXPECT_STREQ(assets.error().what(), expected_error.error().what());
    }
  });

const GameTestRegistration assetsConstAnimationCachesRefreshFramesAndParts("AssetsTest", "ConstAnimationCachesRefreshFramesAndParts", [](GameTest&) {
  auto config = Json::parse(R"({
    "stateTypes": {
      "movement": {
        "default": "walk",
        "states": {
          "walk": {
            "frames": 2,
            "cycle": 1.0,
            "mode": "loop",
            "frameProperties": {"stateFrame": [10, 20]}
          }
        }
      }
    },
    "parts": {
      "body": {
        "properties": {"base": true},
        "partStates": {
          "movement": {
            "walk": {"frameProperties": {"partFrame": [30, 40]}}
          }
        }
      }
    }
  })");
  ASSERT_TRUE(config);
  auto created = AnimatedPartSet::create(*config, 1);
  ASSERT_TRUE(created);
  auto animation = std::move(*created);
  ASSERT_TRUE(animation.update(0.75f));
  AnimatedPartSet const frozen = animation;
  auto partResult = frozen.activePart("body");
  ASSERT_TRUE(partResult);
  auto const& part = partResult->get();
  ASSERT_TRUE(part.activeState);
  EXPECT_EQ(part.activeState->frame, 1u);
  auto partFrame = part.properties.get("partFrame");
  ASSERT_TRUE(partFrame);
  auto partFrameValue = partFrame->get().toInt();
  ASSERT_TRUE(partFrameValue);
  EXPECT_EQ(*partFrameValue, 40);
  auto nextPartFrame = part.nextProperties.get("partFrame");
  ASSERT_TRUE(nextPartFrame);
  auto nextPartFrameValue = nextPartFrame->get().toInt();
  ASSERT_TRUE(nextPartFrameValue);
  EXPECT_EQ(*nextPartFrameValue, 30);
  auto stateResult = frozen.activeState("movement");
  ASSERT_TRUE(stateResult);
  auto const& state = stateResult->get();
  EXPECT_EQ(state.frame, 1u);
  EXPECT_EQ(state.nextFrame, 0u);
  EXPECT_FLOAT_EQ(state.frameProgress, 0.5f);
  auto stateFrame = state.properties.get("stateFrame");
  ASSERT_TRUE(stateFrame);
  auto stateFrameValue = stateFrame->get().toInt();
  ASSERT_TRUE(stateFrameValue);
  EXPECT_EQ(*stateFrameValue, 20);

  AnimatedPartSet const callbackCopy = animation;
  unsigned stateCount = 0;
  ASSERT_TRUE(callbackCopy.forEachActiveState([&](String const& name, auto const& active) -> std::expected<void, StarException> {
    EXPECT_EQ(name, "movement");
    EXPECT_EQ(active.frame, 1u);
    ++stateCount;
    return {};
  }));
  unsigned partCount = 0;
  ASSERT_TRUE(callbackCopy.forEachActivePart([&](String const& name, auto const& active) -> std::expected<void, StarException> {
    EXPECT_EQ(name, "body");
    auto frame = active.properties.get("partFrame");
    if (!frame)
      return std::unexpected(std::move(frame.error()));
    auto value = frame->get().toInt();
    if (!value)
      return std::unexpected(std::move(value.error()));
    EXPECT_EQ(*value, 40);
    ++partCount;
    return {};
  }));
  EXPECT_EQ(stateCount, 1u);
  EXPECT_EQ(partCount, 1u);

  auto writablePart = animation.parts().get("body");
  ASSERT_TRUE(writablePart);
  writablePart->get().partProperties["base"] = false;
  writablePart->get().activePartDirty = true;
  auto updatedPart = animation.activePart("body");
  ASSERT_TRUE(updatedPart);
  auto base = updatedPart->get().properties.get("base");
  ASSERT_TRUE(base);
  auto baseValue = base->get().toBool();
  ASSERT_TRUE(baseValue);
  EXPECT_FALSE(*baseValue);
  ASSERT_TRUE(animation.setStateTypeEnabled("movement", false));
  auto disabledPart = animation.activePart("body");
  ASSERT_TRUE(disabledPart);
  EXPECT_FALSE(disabledPart->get().activeState);
});

const GameTestRegistration
  assetsAnimationTransformCenters("AssetsTest", "TransformCentersPreserveValidationAndRecovery", [](GameTest&) {
    auto config = Json::parse(R"({"parts":{"body":{"properties":{"transforms":[["reset"],["scale",[2,3]]]}}}})");
    ASSERT_TRUE(config);
    auto animation = AnimatedPartSet::create(*config, 1);
    ASSERT_TRUE(animation);
    auto part = animation->activePart("body");
    ASSERT_TRUE(part);
    EXPECT_FLOAT_EQ(part->get().xScaleAnimation, 2);
    EXPECT_FLOAT_EQ(part->get().yScaleAnimation, 3);
    EXPECT_FLOAT_EQ(part->get().xTranslationAnimation, 0);
    EXPECT_FLOAT_EQ(part->get().yTranslationAnimation, 0);
    auto definition = animation->parts().get("body");
    ASSERT_TRUE(definition);
    definition->get().partProperties["transforms"] =
      JsonArray{JsonArray{"reset"}, JsonArray{"scale", JsonArray{2, 3}, JsonArray{1, 2}}};
    definition->get().partProperties["scalingCenter"] = JsonArray{"unused invalid coordinate"};
    definition->get().activePartDirty = true;
    auto explicitCenter = animation->activePart("body");
    ASSERT_TRUE(explicitCenter);
    EXPECT_FLOAT_EQ(explicitCenter->get().xTranslationAnimation, -1);
    EXPECT_FLOAT_EQ(explicitCenter->get().yTranslationAnimation, -4);
    // A supplied default's array type is still checked before using an explicit center.
    definition->get().partProperties["scalingCenter"] = 7;
    definition->get().activePartDirty = true;
    auto invalidDefault = animation->activePart("body");
    ASSERT_FALSE(invalidDefault);
    EXPECT_TRUE(invalidDefault.error().is<JsonException>());
    definition->get().partProperties["scalingCenter"] = JsonArray{0, 0};
    definition->get().activePartDirty = true;
    auto recovered = animation->activePart("body");
    ASSERT_TRUE(recovered);
    EXPECT_FLOAT_EQ(recovered->get().xTranslationAnimation, -1);
    EXPECT_FLOAT_EQ(recovered->get().yTranslationAnimation, -4);
  });

const GameTestRegistration assetsSampleDiscreteProperties(
  "AssetsTest", "SamplePropertiesPreservePriorityFramesAndContinuousPose", [](GameTest&) {
    auto config = Json::parse(R"({
      "stateTypes": {
        "movement": {
          "priority": 1, "default": "walk", "properties": {"value": "type", "typeOnly": true},
          "states": {
            "walk": {
              "frames": 2, "cycle": 1, "mode": "loop", "properties": {"value": "state"},
              "frameProperties": {"value": ["state0", "state1"]}
            },
            "idle": {"properties": {"value": "idle"}}
          }
        },
        "overlay": {
          "priority": 2, "enabled": false, "default": "on",
          "states": {"on": {}, "invalid": {"frameProperties": {"unused": 7}}}
        }
      },
      "parts": {
        "body": {
          "properties": {"value": "base", "baseOnly": true, "interpolated": true},
          "partStates": {
            "movement": {
              "walk": {
                "properties": {"value": "part"},
                "frameProperties": {
                  "value": ["part0", "part1"],
                  "transforms": [[["reset"], ["translate", [0, 0]]], [["reset"], ["translate", [10, 0]]]]
                }
              },
              "idle": {"properties": {"value": "idlePart"}}
            },
            "overlay": {"on": {"properties": {"value": "overlay"}}}
          }
        }
      }
    })");
    ASSERT_TRUE(config);
    auto animation = AnimatedPartSet::create(*config, 1);
    ASSERT_TRUE(animation);
    ASSERT_TRUE(animation->update(0.4f));
    AnimatedPartSet::Sample early;
    AnimatedPartSet::Sample late;
    auto check = [&](AnimatedPartSet::Sample& sample, unsigned frame, String const& value, String const& next, float translation) {
      auto result = animation->activePart("body", &sample);
      ASSERT_TRUE(result);
      auto const& part = result->get();
      ASSERT_TRUE(part.activeState);
      EXPECT_EQ(part.activeState->frame, frame);
      auto currentValue = part.properties.get("value");
      auto nextValue = part.nextProperties.get("value");
      ASSERT_TRUE(currentValue);
      ASSERT_TRUE(nextValue);
      EXPECT_EQ(currentValue->get(), Json(value));
      EXPECT_EQ(nextValue->get(), Json(next));
      EXPECT_NEAR(part.xTranslationAnimation, translation, 0.0001f);
      auto base = part.properties.get("baseOnly");
      ASSERT_TRUE(base);
      EXPECT_EQ(base->get(), Json(true));
    };
    animation->prepareSample(early, 0.25f);
    animation->prepareSample(late, 0.75f);
    check(early, 0, "part0", "part1", 2);
    check(late, 0, "part0", "part1", 6);
    check(early, 0, "part0", "part1", 2);
    animation->prepareSample(early, 0.5f);
    check(early, 0, "part0", "part1", 4);
    auto state = animation->activeState("movement", &early);
    ASSERT_TRUE(state);
    auto stateValue = state->get().properties.get("value");
    auto nextStateValue = state->get().nextProperties.get("value");
    ASSERT_TRUE(stateValue);
    ASSERT_TRUE(nextStateValue);
    EXPECT_EQ(stateValue->get(), Json("state0"));
    EXPECT_EQ(nextStateValue->get(), Json("state1"));
    auto typeOnly = state->get().properties.get("typeOnly");
    ASSERT_TRUE(typeOnly);
    EXPECT_EQ(typeOnly->get(), Json(true));

    ASSERT_TRUE(animation->setPresentationState("movement", "idle", false, false, early));
    check(early, 0, "idlePart", "idlePart", 0);
    check(late, 0, "part0", "part1", 6);
    auto logical = animation->activeState("movement");
    ASSERT_TRUE(logical);
    EXPECT_EQ(logical->get().stateName, "walk");
    animation->prepareSample(early, 0.5f);
    check(early, 0, "part0", "part1", 4);

    ASSERT_TRUE(animation->update(0.4f));
    animation->prepareSample(early, 0.5f);
    check(early, 1, "part1", "part0", 8);
    ASSERT_TRUE(animation->setActiveState("movement", "walk", true, true));
    ASSERT_TRUE(animation->update(0.4f));
    animation->prepareSample(early, 0.25f);
    check(early, 1, "part1", "part0", 8);
    animation->prepareSample(early, 0.75f);
    check(early, 1, "part1", "part0", 4);
    auto reversed = animation->activePart("body", &early);
    ASSERT_TRUE(reversed);
    EXPECT_TRUE(reversed->get().activeState->reverse);

    ASSERT_TRUE(animation->setStateTypeEnabled("overlay", true));
    animation->prepareSample(early, 0.5f);
    check(early, 0, "overlay", "overlay", 0);
    ASSERT_TRUE(animation->setPresentationState("overlay", "invalid", false, false, early));
    auto invalid = animation->activePart("body", &early);
    ASSERT_FALSE(invalid);
    EXPECT_TRUE(invalid.error().is<JsonException>());
    // Preparing again discards the local override, including its failed cache.
    animation->prepareSample(early, 0.5f);
    check(early, 0, "overlay", "overlay", 0);
    ASSERT_TRUE(animation->setStateTypeEnabled("overlay", false));
    animation->prepareSample(early, 0.5f);
    check(early, 1, "part1", "part0", 6);
  });

const GameTestRegistration assetsSampleRetainedMutableDefinitions(
  "AssetsTest", "SamplePropertiesRecoverFromRetainedEditsAndOwnerReplacement", [](GameTest&) {
    auto config = Json::parse(R"({
      "parts":{"body":{"properties":{"value": 1, "transforms":[["reset"],["scale",[2,3]]]}}}
    })");
    auto otherConfig = Json::parse(R"({"parts":{"body":{"properties":{"value": 9}}}})");
    ASSERT_TRUE(config);
    ASSERT_TRUE(otherConfig);
    auto animation = AnimatedPartSet::create(*config, 1);
    auto other = AnimatedPartSet::create(*otherConfig, 1);
    ASSERT_TRUE(animation);
    ASSERT_TRUE(other);
    AnimatedPartSet::Sample sample;
    auto check = [&](AnimatedPartSet& owner, int expected) {
      owner.prepareSample(sample, 0.5f);
      auto result = owner.activePart("body", &sample);
      ASSERT_TRUE(result);
      auto value = result->get().properties.get("value");
      ASSERT_TRUE(value);
      EXPECT_EQ(value->get(), Json(expected));
    };
    check(*animation, 1);
    check(*other, 9);
    check(*animation, 1);
    auto& retainedParts = animation->parts();
    auto definition = retainedParts.get("body");
    ASSERT_TRUE(definition);
    definition->get().partProperties["value"] = 2;
    definition->get().activePartDirty = true;
    check(*animation, 2);
    // No further parts() call: a reference escaped before the previous prepare.
    definition->get().partProperties["value"] = 3;
    definition->get().partProperties["scalingCenter"] = 7;
    definition->get().activePartDirty = true;
    animation->prepareSample(sample, 0.5f);
    auto invalid = animation->activePart("body", &sample);
    ASSERT_FALSE(invalid);
    EXPECT_TRUE(invalid.error().is<JsonException>());
    definition->get().partProperties["scalingCenter"] = JsonArray{0, 0};
    definition->get().activePartDirty = true;
    // A failed query remains retryable even without preparing again.
    auto recovered = animation->activePart("body", &sample);
    ASSERT_TRUE(recovered);
    auto recoveredValue = recovered->get().properties.get("value");
    ASSERT_TRUE(recoveredValue);
    EXPECT_EQ(recoveredValue->get(), Json(3));
    EXPECT_FLOAT_EQ(recovered->get().xScaleAnimation, 2);
    check(*animation, 3);
    AnimatedPartSet copy = *animation;
    check(copy, 3);
    definition->get().partProperties["value"] = 4;
    definition->get().activePartDirty = true;
    check(*animation, 4);
    check(copy, 3);

    // Assignment must not forget the escaped map reference in the destination,
    // even when its source has never exposed mutable definitions.
    *animation = *other;
    check(*animation, 9);
    auto replacement = retainedParts.get("body");
    ASSERT_TRUE(replacement);
    replacement->get().partProperties["value"] = 10;
    replacement->get().activePartDirty = true;
    check(*animation, 10);
    auto fresh = AnimatedPartSet::create(*otherConfig, 1);
    ASSERT_TRUE(fresh);
    *animation = std::move(*fresh);
    check(*animation, 9);
    replacement = retainedParts.get("body");
    ASSERT_TRUE(replacement);
    replacement->get().partProperties["value"] = 11;
    replacement->get().activePartDirty = true;
    check(*animation, 11);
    check(*other, 9);
  });

const GameTestRegistration assetsSampleDeferredFrameValidation(
  "AssetsTest", "SampleOnlyValidatesTransformFramesWhenConsumed", [](GameTest&) {
    auto config = Json::parse(R"({
      "stateTypes":{"movement":{"default":"walk","states":{"walk":{"frames":3,"cycle":3,"mode":"loop"}}}},
      "parts":{"body":{
        "properties":{"interpolated":false},
        "partStates":{"movement":{"walk":{"frameProperties":{
          "transforms":[[["reset"],["translate",[0,0]]],[["reset"],["translate",[10,0]]],7]
        }}}}
      }}
    })");
    ASSERT_TRUE(config);
    auto animation = AnimatedPartSet::create(*config, 1);
    ASSERT_TRUE(animation);
    ASSERT_TRUE(animation->update(1.25f));
    AnimatedPartSet::Sample sample;
    animation->prepareSample(sample, 1);
    auto currentOnly = animation->activePart("body", &sample);
    ASSERT_TRUE(currentOnly);
    EXPECT_FLOAT_EQ(currentOnly->get().xTranslationAnimation, 10);
    animation->prepareSample(sample, 1);
    ASSERT_TRUE(animation->activePart("body", &sample));
    auto definition = animation->parts().get("body");
    ASSERT_TRUE(definition);
    definition->get().partProperties["interpolated"] = true;
    definition->get().activePartDirty = true;
    animation->prepareSample(sample, 1);
    auto invalidNext = animation->activePart("body", &sample);
    ASSERT_FALSE(invalidNext);
    EXPECT_TRUE(invalidNext.error().is<JsonException>());
    definition->get().partProperties["interpolated"] = false;
    definition->get().activePartDirty = true;
    auto recovered = animation->activePart("body", &sample);
    ASSERT_TRUE(recovered);
    EXPECT_FLOAT_EQ(recovered->get().xTranslationAnimation, 10);
    ASSERT_TRUE(animation->update(1));
    animation->prepareSample(sample, 1);
    auto invalidCurrent = animation->activePart("body", &sample);
    ASSERT_FALSE(invalidCurrent);
    EXPECT_TRUE(invalidCurrent.error().is<JsonException>());
    ASSERT_TRUE(animation->setStateTypeEnabled("movement", false));
    animation->prepareSample(sample, 1);
    auto unused = animation->activePart("body", &sample);
    ASSERT_TRUE(unused);
    EXPECT_FALSE(unused->get().activeState);
    EXPECT_FLOAT_EQ(unused->get().xTranslationAnimation, 0);
  });

const GameTestRegistration effectSourceParticleCache(
  "EffectSourceTest", "LazyParticleChoicesRetainOwnedResultsAndStageErrors", [](GameTest& fixture) {
    Json definition = JsonObject{{"duration", 1},
                                 {"durationVariance", 0},
                                 {"loops", true},
                                 {"start", JsonObject{{"particles", JsonArray{JsonArray{"start-a", "start-b"}}}}},
                                 {"particles", JsonArray{JsonArray{"loop"}}},
                                 {"stop", JsonObject{{"particles", JsonArray{7}}}}};
    auto source = EffectSource::create(fixture.root(), "private-cache-fixture", "normal", definition);
    ASSERT_TRUE(source);
    auto initial = (*source)->particles();
    ASSERT_TRUE(initial);
    EXPECT_EQ(*initial, (List<String>{"start-a", "start-b"}));
    (*initial)[0] = "caller-owned change";
    auto repeated = (*source)->particles();
    ASSERT_TRUE(repeated);
    EXPECT_EQ(*repeated, (List<String>{"start-a", "start-b"}));
    (*source)->finishEventCollection();
    (*source)->tick(2);
    auto loop = (*source)->particles();
    ASSERT_TRUE(loop);
    EXPECT_EQ(*loop, (List<String>{"loop"}));
    (*source)->finishEventCollection();
    auto afterCollection = (*source)->particles();
    ASSERT_TRUE(afterCollection);
    EXPECT_EQ(*afterCollection, List<String>{});
    (*source)->stop();
    (*source)->tick(0);
    auto final = (*source)->particles();
    ASSERT_FALSE(final);
    EXPECT_TRUE(final.error().is<JsonException>());
    auto repaired = definition.set("stop", JsonObject{{"particles", JsonArray{JsonArray{"stop"}}}});
    ASSERT_TRUE(repaired);
    auto replacement = EffectSource::create(fixture.root(), "private-cache-fixture", "normal", *repaired);
    ASSERT_TRUE(replacement);
    (*replacement)->finishEventCollection();
    (*replacement)->stop();
    (*replacement)->tick(0);
    auto freshFinal = (*replacement)->particles();
    ASSERT_TRUE(freshFinal);
    EXPECT_EQ(*freshFinal, (List<String>{"stop"}));
  });

const GameTestRegistration assetsWavSignaturesAndPcmSamples("AssetsTest", "WavSignaturesAndPcmSamples", [](GameTest&) {
  std::array<unsigned char, 48> bytes = {
    'R', 'I', 'F', 'F', 40, 0, 0, 0, 'W', 'A', 'V', 'E',
    'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 1, 0,
    0x40, 0x1f, 0, 0, 0x80, 0x3e, 0, 0, 2, 0, 16, 0,
    'd', 'a', 't', 'a', 4, 0, 0, 0, 0x34, 0x12, 0xfe, 0xff
  };
  auto device = [&]() -> std::expected<std::shared_ptr<Buffer>, StarException> {
    auto buffer = std::make_shared<Buffer>();
    auto written = buffer->write_full({reinterpret_cast<char const*>(bytes.data()), bytes.size()});
    if (!written)
      return std::unexpected(std::move(written.error()));
    auto sought = buffer->seek(0);
    if (!sought)
      return std::unexpected(std::move(sought.error()));
    return buffer;
  };
  auto input = device();
  ASSERT_TRUE(input);
  auto audio = Audio::create(*input, "signature-regression.wav");
  ASSERT_TRUE(audio);
  EXPECT_FALSE(audio->compressed());
  EXPECT_EQ(audio->channels(), 1u);
  EXPECT_EQ(audio->sampleRate(), 8000u);
  std::array<std::int16_t, 2> samples{};
  auto read = audio->read(samples);
  ASSERT_TRUE(read);
  ASSERT_EQ(*read, samples.size());
  EXPECT_EQ(samples[0], 0x1234);
  EXPECT_EQ(samples[1], -2);

  ASSERT_TRUE(audio->seekSample(1));
  auto cloned = audio->clone();
  ASSERT_TRUE(cloned);
  ASSERT_TRUE(audio->seekSample(0));
  EXPECT_EQ(cloned->currentSample(), 1u);
  Audio moved(std::move(*audio));
  *audio = std::move(*cloned);
  // Replacing the original decoder must not invalidate a clone's shared PCM storage.
  ASSERT_TRUE(audio->seekSample(0));
  moved = std::move(*audio);
  EXPECT_FALSE(moved.seekSample(3));
  EXPECT_EQ(moved.currentSample(), 0u);
  read = moved.read(samples);
  ASSERT_TRUE(read);
  EXPECT_EQ(*read, samples.size());
  EXPECT_EQ(samples[0], 0x1234);
  EXPECT_EQ(samples[1], -2);

  // High-bit signature bytes must be classified as unsigned characters.
  bytes[12] = 0xff;
  auto invalidFmtInput = device();
  ASSERT_TRUE(invalidFmtInput);
  auto invalidFmt = Audio::create(*invalidFmtInput, "invalid-fmt.wav");
  ASSERT_FALSE(invalidFmt);
  EXPECT_TRUE(invalidFmt.error().is<AudioException>());
  bytes[12] = 'f';
  bytes[36] = 0x80;
  auto invalidDataInput = device();
  ASSERT_TRUE(invalidDataInput);
  auto invalidData = Audio::create(*invalidDataInput, "invalid-data.wav");
  ASSERT_FALSE(invalidData);
  EXPECT_TRUE(invalidData.error().is<AudioException>());
});

const GameTestRegistration audioMixerGroupBindings("AssetsTest", "AudioMixerGroupLuaContract", [](GameTest& fixture) {
  auto assets = fixture.root().assets();
  ASSERT_TRUE(assets);
  auto input = (*assets)->openFile("/sfx/interface/voice_on.ogg");
  ASSERT_TRUE(input);
  auto audio = Audio::create(*input, "mixer-group.ogg");
  ASSERT_TRUE(audio);
  auto instance = AudioInstance::create(*audio);
  ASSERT_TRUE(instance);
  auto engine = LuaEngine::create();
  ASSERT_TRUE(engine);
  auto context = (*engine)->createContext();
  ASSERT_TRUE(context);
  ASSERT_TRUE(context->set("audio", *instance));
  ASSERT_TRUE(context->load(R"LUA(
    function selectGroup(group)
      audio:setMixerGroup(group)
      return audio:mixerGroup()
    end
  )LUA"));
  struct GroupCase {
    std::string_view input;
    std::string_view canonical;
    MixerGroup group;
  };
  for (auto const& [inputName, canonical, group] :
       std::to_array<GroupCase>({{"eFfEcTs", "Effects", MixerGroup::Effects},
                                 {"MUSIC", "Music", MixerGroup::Music},
                                 {"cinematic", "Cinematic", MixerGroup::Cinematic},
                                 {"Instruments", "Instruments", MixerGroup::Instruments}})) {
    auto selected = context->invokePath<String>("selectGroup", String(inputName));
    ASSERT_TRUE(selected);
    EXPECT_EQ((*instance)->mixerGroup(), group);
    EXPECT_EQ(selected->utf8(), canonical);
  }
  for (auto const& invalid : {String("unknown"), String("Músic"), String(std::string("Music\0Effects", 13)),
                              String(std::string(1, static_cast<char>(0xff)))}) {
    auto selected = context->invokePath<String>("selectGroup", invalid);
    EXPECT_FALSE(selected);
    EXPECT_EQ((*instance)->mixerGroup(), MixerGroup::Instruments);
  }
  auto recovered = context->invokePath<String>("selectGroup", String("effects"));
  ASSERT_TRUE(recovered);
  EXPECT_EQ(*recovered, "Effects");
  EXPECT_EQ((*instance)->mixerGroup(), MixerGroup::Effects);
});

const GameTestRegistration assetsOggCloneLifetimes("AssetsTest", "OggCloneIndependentCursorsAndLifetime", [](GameTest& fixture) {
  auto assets = fixture.root().assets();
  ASSERT_TRUE(assets);
  auto input = (*assets)->openFile("/sfx/interface/voice_on.ogg");
  ASSERT_TRUE(input);
  auto created = Audio::create(std::move(*input), "clone-lifetime.ogg");
  ASSERT_TRUE(created);
  std::optional<Audio> owner(std::move(*created));
  ASSERT_TRUE(owner->compressed());
  auto total = owner->totalSamples();
  ASSERT_TRUE(total);
  ASSERT_GT(*total, 0u);
  ASSERT_TRUE(owner->seekSample(*total / 2));
  auto cloned = owner->clone();
  ASSERT_TRUE(cloned);
  EXPECT_EQ(cloned->currentSample(), *total / 2);
  ASSERT_TRUE(owner->seekSample(0));
  EXPECT_EQ(cloned->currentSample(), *total / 2);

  // A rejected seek leaves the original decoder usable at its existing cursor.
  EXPECT_FALSE(owner->seekSample(*total + 1));
  EXPECT_EQ(owner->currentSample(), 0u);
  std::array<std::int16_t, 64> original{};
  auto read = owner->read(original);
  ASSERT_TRUE(read);
  ASSERT_GT(*read, 0u);
  owner.reset();
  ASSERT_TRUE(cloned->seekSample(0));
  std::array<std::int16_t, 64> replay{};
  auto replayed = cloned->read(replay);
  ASSERT_TRUE(replayed);
  EXPECT_EQ(*replayed, *read);
  EXPECT_EQ(replay, original);

  ASSERT_TRUE(cloned->seekSample(0));
  ASSERT_TRUE(cloned->uncompress());
  EXPECT_FALSE(cloned->compressed());
  ASSERT_TRUE(cloned->seekSample(0));
  auto pcmClone = cloned->clone();
  ASSERT_TRUE(pcmClone);
  Audio moved(std::move(*pcmClone));
  *cloned = std::move(moved);
  replay.fill(0);
  replayed = cloned->read(replay);
  ASSERT_TRUE(replayed);
  EXPECT_EQ(*replayed, *read);
  EXPECT_EQ(replay, original);
});

const GameTestRegistration
  assetsFailedStartupJoinsWorkers("AssetsTest", "FailedStartupJoinsWorkers", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto tempDir = std::move(*directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::remove_all(tempDir)); });
    Assets::Settings settings{};
    settings.workerPoolSize = 4;
    settings.assetTimeToLive = 10;
    // Fail after worker launch, including completed preload work followed by failure.
    Logger logger;
    for (unsigned attempt = 0; attempt < 4; ++attempt) {
      auto assets = Assets::create(logger, fixture.root().random(), fixture.root().logMap(), settings, {tempDir});
      ASSERT_FALSE(assets);
      EXPECT_TRUE(assets.error().is<AssetException>());
    }

    ASSERT_TRUE(File::write_file(String("not json"), File::relative_to(tempDir, "preload.config")));
    auto malformed = Assets::create(logger, fixture.root().random(), fixture.root().logMap(), settings, {tempDir});
    ASSERT_FALSE(malformed);
    EXPECT_TRUE(malformed.error().is<AssetException>());

    ASSERT_TRUE(File::write_file(String("{}"), File::relative_to(tempDir, "present.json")));
    ASSERT_TRUE(File::write_file(
      String("[{\"type\":\"json\",\"path\":\"/present.json\"},{\"type\":\"json\",\"path\":\"/absent.json\"}]"),
      File::relative_to(tempDir, "preload.config")));
    auto missing = Assets::create(logger, fixture.root().random(), fixture.root().logMap(), settings, {tempDir});
    ASSERT_FALSE(missing);
    EXPECT_TRUE(missing.error().is<AssetException>());

    ASSERT_TRUE(File::write_file(String("[{\"type\":\"image\",\"path\":\"relative.png\"}]"),
                                 File::relative_to(tempDir, "preload.config")));
    auto invalid = Assets::create(logger, fixture.root().random(), fixture.root().logMap(), settings, {tempDir});
    ASSERT_FALSE(invalid);
    EXPECT_TRUE(invalid.error().is<AssetException>());
    ASSERT_TRUE(File::write_file(String("[]"), File::relative_to(tempDir, "preload.config")));
    auto valid = Assets::create(logger, fixture.root().random(), fixture.root().logMap(), settings, {tempDir});
    ASSERT_TRUE(valid);
    auto recovered = (*valid)->json("/present.json");
    ASSERT_TRUE(recovered);
    EXPECT_EQ(*recovered, Json(JsonObject{}));
  });

const GameTestRegistration
  assetsShutdownWithQueuedImageDependencies("AssetsTest", "ShutdownWithQueuedImageDependencies", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto tempDir = std::move(*directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::remove_all(tempDir)); });
    ASSERT_TRUE(File::write_file(String("[]"), File::relative_to(tempDir, "preload.config")));
    auto bytes = pngTestBytes(fixture.root().logger());
    ASSERT_TRUE(bytes);
    ASSERT_TRUE(File::write_file(*bytes, File::relative_to(tempDir, "sample.png")));
    Assets::Settings settings{};
    settings.workerPoolSize = 4;
    settings.assetTimeToLive = 10;
    for (unsigned attempt = 0; attempt < 16; ++attempt) {
      auto assets =
        Assets::create(fixture.root().logger(), fixture.root().random(), fixture.root().logMap(), settings, {tempDir});
      ASSERT_TRUE(assets);
      ASSERT_TRUE((*assets)->queueImages(StringList{"/sample.png?flipx", "/sample.png?flipy", "/sample.png"}));
    }
  });

const GameTestRegistration
  assetsQueuedLuaPatches("AssetsTest", "QueuedLuaPatchErrorsDoNotDiscardOtherResults", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    ASSERT_TRUE(File::write_file(String("[]"), File::relative_to(*directory, "preload.config")));
    ASSERT_TRUE(File::write_file(String("{}"), File::relative_to(*directory, "broken.json")));
    ASSERT_TRUE(File::write_file(String("function patch() error('deliberate patch failure') end"),
                                 File::relative_to(*directory, "broken.json.patch.lua")));
    StringList paths{"/broken.json"};
    for (unsigned i = 0; i != 32; ++i) {
      auto name = Star::format("queued{}.json", i);
      ASSERT_TRUE(File::write_file(String(Star::format("{{\"index\":{}}}", i)), File::relative_to(*directory, name)));
      ASSERT_TRUE(File::write_file(String(R"lua(function patch(asset)
      local garbage = {}
      for i = 1, 128 do
        local node = {value = i}
        node.self = node
        garbage[i] = node
      end
      asset.result = asset.index + garbage[128].value
      return asset
    end)lua"),
                                   File::relative_to(*directory, name + ".patch.lua")));
      paths.push_back("/" + name);
    }
    Assets::Settings settings{};
    settings.workerPoolSize = 4;
    settings.assetTimeToLive = 600;
    Logger logger;
    auto assets = Assets::create(logger, fixture.root().random(), fixture.root().logMap(), settings, {*directory});
    ASSERT_TRUE(assets);
    ASSERT_TRUE((*assets)->queueJsons(paths));
    EXPECT_FALSE((*assets)->json("/broken.json"));
    for (unsigned i = 0; i != 32; ++i) {
      auto result = (*assets)->json(paths[i + 1]);
      ASSERT_TRUE(result);
      EXPECT_EQ(*result, Json(JsonObject{{"index", i}, {"result", i + 128}}));
    }
    EXPECT_FALSE((*assets)->json("/broken.json"));
  });

const GameTestRegistration assetsSharedPatchStateAcrossDrains(
  "AssetsTest", "SharedPatchStateSurvivesQueueDrainsAndErrors", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    ASSERT_TRUE(File::write_file(String("[]"), File::relative_to(*directory, "preload.config")));
    StringList paths;
    JsonArray targets;
    for (unsigned i = 0; i <= 32; ++i) {
      auto name = Star::format("shared{}.json", i);
      ASSERT_TRUE(File::write_file(String(Star::format("{{\"index\":{}}}", i)), File::relative_to(*directory, name)));
      paths.push_back("/" + name);
      targets.emplace_back(paths.back());
    }
    Json patchList = JsonArray{JsonObject{{"paths", std::move(targets)}, {"patches", JsonArray{"/shared.lua"}}}};
    auto encoded = patchList.repr();
    ASSERT_TRUE(encoded);
    // Patch lists attach to already registered targets in source scan order.
    ASSERT_TRUE(File::write_file(*encoded, File::relative_to(*directory, "zz-shared.patchlist")));
    ASSERT_TRUE(File::write_file(String(R"lua(counter = 0
function patch(asset)
  counter = counter + 1
  local garbage = {}
  for i = 1, 128 do
    local node = {value = i}
    node.self = node
    garbage[i] = node
  end
  if asset.index == 17 then error("shared patch failure") end
  asset.serial = counter
  return asset
end
)lua"),
                                 File::relative_to(*directory, "shared.lua")));
    for (unsigned workerCount : {1u, 4u}) {
      Assets::Settings settings{};
      settings.workerPoolSize = workerCount;
      settings.assetTimeToLive = 600;
      Logger logger;
      auto assets = Assets::create(logger, fixture.root().random(), fixture.root().logMap(), settings, {*directory});
      ASSERT_TRUE(assets);
      std::set<std::int64_t> serials;
      for (unsigned begin = 0; begin < 32; begin += 8) {
        StringList batch;
        for (unsigned i = begin; i < begin + 8; ++i)
          batch.push_back(paths[i]);
        ASSERT_TRUE((*assets)->queueJsons(batch));
        auto drained = AssetWorkerTestAccess::drain(**assets);
        ASSERT_TRUE(drained) << drained.error().what();
        for (unsigned i = begin; i < begin + 8; ++i) {
          auto loaded = (*assets)->json(paths[i]);
          if (i == 17) {
            EXPECT_FALSE(loaded);
            continue;
          }
          ASSERT_TRUE(loaded) << loaded.error().what();
          auto serial = loaded->getInt("serial");
          ASSERT_TRUE(serial);
          EXPECT_GE(*serial, 1);
          EXPECT_LE(*serial, 32);
          EXPECT_TRUE(serials.insert(*serial).second);
        }
      }
      EXPECT_EQ(serials.size(), 31u);
      auto final = (*assets)->json(paths[32]);
      ASSERT_TRUE(final) << final.error().what();
      auto count = final->getInt("serial");
      ASSERT_TRUE(count);
      EXPECT_EQ(*count, 33);
      EXPECT_FALSE((*assets)->json(paths[17]));
      assets->reset();
    }
  });

const GameTestRegistration pngRoundTripPreservesPixelsAndMetadata("ImageTest", "PngRoundTripPreservesPixelsAndMetadata", [](GameTest& fixture) {
  for (auto format : {PixelFormat::RGB24, PixelFormat::RGBA32}) {
    auto image = pngTestImage(format);
    ASSERT_TRUE(image);
    auto buffer = std::make_shared<Buffer>();
    ASSERT_TRUE(image->writePng(fixture.root().logger(), buffer));
    ASSERT_TRUE(buffer->seek(0));
    auto metadata = Image::readPngMetadata(fixture.root().logger(), buffer);
    ASSERT_TRUE(metadata);
    auto [size, decodedFormat] = *metadata;
    EXPECT_EQ(size, image->size());
    EXPECT_EQ(decodedFormat, format);
    ASSERT_TRUE(buffer->seek(0));
    auto decoded = Image::readPng(fixture.root().logger(), buffer);
    ASSERT_TRUE(decoded);
    EXPECT_EQ(decoded->pixelFormat(), format);
    for (unsigned y = 0; y < 2; ++y)
      for (unsigned x = 0; x < 2; ++x) {
        auto actual = decoded->get(x, y);
        ASSERT_TRUE(actual);
        auto expected = image->get(x, y);
        ASSERT_TRUE(expected);
        EXPECT_EQ(*actual, *expected);
      }
  }
});

const GameTestRegistration damagedPngHeaderAndImageDataThrow("ImageTest", "DamagedPngHeaderAndImageDataThrow", [](GameTest& fixture) {
  auto encoded = pngTestBytes(fixture.root().logger());
  ASSERT_TRUE(encoded);
  auto bytes = std::move(*encoded);
  ASSERT_GT(bytes.size(), 41u);
  ASSERT_EQ(std::memcmp(bytes.span().data() + 37, "IDAT", 4), 0);
  auto badHeader = bytes;
  badHeader[29] ^= 1; // IHDR CRC: signature intact, metadata parsing fails.
  auto badPixels = bytes;
  badPixels[41] = 0; // Invalid zlib header, reached after Image/rows allocation.
  auto headerMetadata = Image::readPngMetadata(fixture.root().logger(), std::make_shared<Buffer>(badHeader));
  ASSERT_FALSE(headerMetadata);
  EXPECT_TRUE(headerMetadata.error().is<ImageException>());
  auto headerImage = Image::readPng(fixture.root().logger(), std::make_shared<Buffer>(badHeader));
  ASSERT_FALSE(headerImage);
  EXPECT_TRUE(headerImage.error().is<ImageException>());
  auto pixelMetadata = Image::readPngMetadata(fixture.root().logger(), std::make_shared<Buffer>(badPixels));
  ASSERT_TRUE(pixelMetadata);
  auto pixelImage = Image::readPng(fixture.root().logger(), std::make_shared<Buffer>(badPixels));
  ASSERT_FALSE(pixelImage);
  EXPECT_TRUE(pixelImage.error().is<ImageException>());
});

const GameTestRegistration pngCallbacksRethrowOriginalIoException("ImageTest", "PngCallbacksRethrowOriginalIoException", [](GameTest& fixture) {
  auto bytes = pngTestBytes(fixture.root().logger());
  ASSERT_TRUE(bytes);
  auto metadata = std::make_shared<FailingPngBuffer>(*bytes, 8);
  auto metadataResult = Image::readPngMetadata(fixture.root().logger(), metadata);
  ASSERT_FALSE(metadataResult);
  EXPECT_TRUE(metadataResult.error().is<PngIoFailure>());
  EXPECT_EQ(metadata->unwound, 1u);
  auto header = std::make_shared<FailingPngBuffer>(*bytes, 8);
  auto headerResult = Image::readPng(fixture.root().logger(), header);
  ASSERT_FALSE(headerResult);
  EXPECT_TRUE(headerResult.error().is<PngIoFailure>());
  EXPECT_EQ(header->unwound, 1u);
  auto pixels = std::make_shared<FailingPngBuffer>(*bytes, 41);
  auto pixelResult = Image::readPng(fixture.root().logger(), pixels);
  ASSERT_FALSE(pixelResult);
  EXPECT_TRUE(pixelResult.error().is<PngIoFailure>());
  EXPECT_EQ(pixels->unwound, 1u);
  for (StreamOffset failAt : {0, 41}) {
    auto output = std::make_shared<FailingPngBuffer>(ByteArray{}, failAt);
    auto image = pngTestImage(PixelFormat::RGBA32);
    ASSERT_TRUE(image);
    auto written = image->writePng(fixture.root().logger(), output);
    ASSERT_FALSE(written);
    EXPECT_TRUE(written.error().is<PngIoFailure>());
    EXPECT_EQ(output->unwound, 1u);
  }
});

const GameTestRegistration pngErrorCallbackContainsDiagnosticExceptions("ImageTest", "PngErrorCallbackContainsDiagnosticExceptions", [](GameTest& fixture) {
  auto damaged = pngTestBytes(fixture.root().logger());
  ASSERT_TRUE(damaged);
  (*damaged)[29] ^= 1;
  auto metadata = Image::readPngMetadata(fixture.root().logger(), std::make_shared<FailingPngNameBuffer>(*damaged));
  ASSERT_FALSE(metadata);
  EXPECT_TRUE(metadata.error().is<PngIoFailure>());
  auto image = Image::readPng(fixture.root().logger(), std::make_shared<FailingPngNameBuffer>(*damaged));
  ASSERT_FALSE(image);
  EXPECT_TRUE(image.error().is<PngIoFailure>());
});

const GameTestRegistration invalidPngWriteThrows("ImageTest", "InvalidPngWriteThrows", [](GameTest& fixture) {
  auto output = std::make_shared<Buffer>();
  auto written = Image().writePng(fixture.root().logger(), output);
  ASSERT_FALSE(written);
  EXPECT_TRUE(written.error().is<ImageException>());
});

const GameTestRegistration assetsFailureCacheReload(
  "AssetsTest", "QueuedFailureSurvivesCacheClearAndRecoversAfterReload", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    ASSERT_TRUE(File::write_file(String("[]"), File::relative_to(*directory, "preload.config")));
    auto path = File::relative_to(*directory, "failure.json");
    ASSERT_TRUE(File::write_file(String("{ invalid json"), path));
    Assets::Settings settings{};
    settings.workerPoolSize = 2;
    settings.assetTimeToLive = 600;
    Logger logger;
    auto assets = Assets::create(logger, fixture.root().random(), fixture.root().logMap(), settings, {*directory});
    ASSERT_TRUE(assets);
    ASSERT_TRUE((*assets)->queueJsons(StringList{"/failure.json"}));
    ASSERT_TRUE(AssetWorkerTestAccess::drain(**assets));
    auto first = (*assets)->json("/failure.json");
    ASSERT_FALSE(first);
    EXPECT_TRUE(first.error().is<AssetException>());
    EXPECT_EQ(first.error().diagnosticSummary()->asset, "/failure.json");
    auto reason = first.error().diagnosticSummary()->reason;
    ASSERT_TRUE(File::write_file(String(R"({"recovered":42})"), path));
    (*assets)->clearCache();
    auto cached = (*assets)->json("/failure.json");
    ASSERT_FALSE(cached);
    EXPECT_EQ(cached.error().diagnosticSummary()->reason, reason);
    (*assets)->hotReload();
    ASSERT_TRUE((*assets)->queueJsons(StringList{"/failure.json"}));
    ASSERT_TRUE(AssetWorkerTestAccess::drain(**assets));
    auto recovered = (*assets)->json("/failure.json");
    ASSERT_TRUE(recovered);
    EXPECT_EQ(recovered->getInt("recovered"), 42);
  });

const GameTestRegistration recoverableDatabaseProvider(
  "AssetsTest", "RecoverableDatabaseFailureReportsProviderAndKeepsFallback", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    ASSERT_TRUE(File::write_file(String("[]"), File::relative_to(*directory, "preload.config")));
    ASSERT_TRUE(File::write_file(String(R"({"name":"dance_recovery","friendlyName":"Dance Recovery Provider"})"),
                                 File::relative_to(*directory, "_metadata")));
    ASSERT_TRUE(File::write_file(String("{}"), File::relative_to(*directory, "broken.dance")));
    ASSERT_TRUE(
      File::write_file(String(R"({"name":"assetmissing","states":[],"cycle":1,"cyclic":true,"duration":1,"steps":[]})"),
                       File::relative_to(*directory, "fallback.dance")));
    class Errors : public LogSink {
    public:
      Errors() { setLevel(LogLevel::Error); }
      std::expected<void, StarException> log(std::string_view message, LogLevel) override {
        messages.append(message);
        return {};
      }
      std::string messages;
    };
    Logger logger;
    logger.removeStdoutSink();
    auto errors = std::make_shared<Errors>();
    logger.addSink(errors);
    Assets::Settings settings{};
    settings.workerPoolSize = 1;
    auto assets = Assets::create(logger, fixture.root().random(), fixture.root().logMap(), settings, {*directory});
    ASSERT_TRUE(assets);
    auto database = DanceDatabase::create(*assets, logger);
    ASSERT_TRUE(database);
    EXPECT_NE(errors->messages.find("/broken.dance"), std::string::npos);
    EXPECT_NE(errors->messages.find("JsonException"), std::string::npos);
    EXPECT_NE(errors->messages.find("Dance Recovery Provider"), std::string::npos);
    EXPECT_NE(errors->messages.find(directory->utf8()), std::string::npos);
    auto recovered = (*database)->getDance("broken");
    ASSERT_TRUE(recovered);
    EXPECT_EQ((*recovered)->name, "assetmissing");
  });
