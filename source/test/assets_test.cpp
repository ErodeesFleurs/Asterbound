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
import star.effect_emitter;
import star.entity_rendering;
import star.particle;
import star.particle_database;
import star.emote_processor;
import star.humanoid;
import star.set;
import star.net_compatibility;
import star.lua;
import star.lua_game_converters;
import star.mixer;
import star.dance_database;
import star.object;
import star.object_database;
import star.tile_damage;
import star.tile_entity;
import star.universe_client;
import star.world_client;
import star.world_server;
import star.net_packets;
import star.warping;
import star.game_types;
import star.drawable;
import star.image_metadata_database;
import star.color;
import star.rect;
import star.line;
import star.matrix3;

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

import star.algorithm;
namespace Star {
struct AssetWorkerTestAccess {
  static bool cached_frame_miss(Assets const& assets, String const& path) {
    std::lock_guard lock(assets.m_assets_mutex);
    auto file = assets.m_frame_files_by_image.find_ref(path);
    return file && file->get().path.empty();
  }

  static List<Assets::AssetId> queued_ids(Assets const& assets) {
    std::lock_guard lock(assets.m_assets_mutex);
    return assets.m_queue.keys();
  }

  static auto take_worker_task(Assets& assets) {
    std::lock_guard lock(assets.m_assets_mutex);
    return assets.next_worker_task();
  }

  static std::expected<std::shared_ptr<Assets::AssetData>, StarException> decode_post(Assets& assets,
      Assets::WorkerTask const& task) {
    if (!task.post)
      return std::unexpected(StarException("Expected an audio post-processing task"));
    return assets.post_process_audio(*task.post);
  }

  static std::expected<bool, StarException> finish_post(Assets& assets, Assets::WorkerTask task,
      std::expected<std::shared_ptr<Assets::AssetData>, StarException> processed) {
    std::lock_guard lock(assets.m_assets_mutex);
    return assets.publish_post(task.id, std::move(*task.post), std::move(processed));
  }

  static std::expected<void, StarException> drain(Assets& assets) {
    std::unique_lock lock(assets.m_assets_mutex);
    if (!assets.m_assets_done.wait_for(lock, std::chrono::seconds(60), [&] {
          return assets.m_queue.empty() || assets.m_worker_error || assets.m_worker_stop_source.stop_requested();
        }))
      return std::unexpected(StarException("Asset queue regression did not drain"));
    if (assets.m_worker_error)
      return std::unexpected(*assets.m_worker_error);
    if (assets.m_worker_stop_source.stop_requested())
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
      for (auto const& pattern : assets.m_settings.digest_ignore) {
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
      auto device = descriptor->get().source->open(descriptor->get().source_name);
      if (!device)
        return std::unexpected(std::move(device.error()));
      auto size = (*device)->size();
      if (!size)
        return std::unexpected(std::move(size.error()));
      auto bytes = DataStreamBuffer::serialize(*size);
      if (!bytes)
        return std::unexpected(std::move(bytes.error()));
      digest.push(*bytes);
      for (auto const& patch : descriptor->get().patch_sources) {
        auto patch_device = patch.second->open(AssetPath::remove_sub_path(patch.first));
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
        EXPECT_EQ(batch[i].error().what(), scalar.error().what());
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
    auto written = image->write_png(logger, buffer);
    if (!written)
      return std::unexpected(std::move(written.error()));
    return buffer->take_data();
  }
}

namespace {
constexpr std::array<unsigned char, 48> pcm_test_wave = {
  'R', 'I', 'F', 'F', 40, 0, 0, 0, 'W', 'A', 'V', 'E',
  'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 1, 0,
  0x40, 0x1f, 0, 0, 0x80, 0x3e, 0, 0, 2, 0, 16, 0,
  'd', 'a', 't', 'a', 4, 0, 0, 0, 0x34, 0x12, 0xfe, 0xff
};

std::expected<std::unique_ptr<Root>, StarException> effect_test_root(GameTest& fixture, String const& directory,
    std::initializer_list<std::pair<String, String>> files, bool include_game_assets = false) {
  auto overlay = File::relative_to(directory, "assets");
  auto created = File::create_directory(overlay);
  if (!created)
    return std::unexpected(std::move(created.error()));
  for (auto const& [name, contents] : files) {
    auto written = File::write_file(contents, File::relative_to(overlay, name));
    if (!written)
      return std::unexpected(std::move(written.error()));
  }
  auto wave = std::bit_cast<std::array<char, pcm_test_wave.size()>>(pcm_test_wave);
  auto audio = File::write_file(std::span<char const>(wave), File::relative_to(overlay, "loop.wav"));
  if (!audio)
    return std::unexpected(std::move(audio.error()));
  if (!include_game_assets) {
    auto preload = File::write_file(String("[]"), File::relative_to(overlay, "preload.config"));
    if (!preload)
      return std::unexpected(std::move(preload.error()));
  }
  auto settings = fixture.rootSettings();
  if (!include_game_assets) {
    settings.assetDirectories.clear();
    settings.assetSources.clear();
  }
  settings.assetSources.push_back(overlay);
  settings.storageDirectory = File::relative_to(directory, "storage");
  settings.runtimeConfigFile = {};
  settings.logFile = {};
  settings.includeUGC = false;
  settings.mix_random_entropy = false;
  settings.assetsSettings.missing_audio = {};
  return Root::create(std::move(settings));
}

struct EffectEventSink : NullRenderCallback {
  std::string accepted;
  std::optional<std::size_t> fail_at;
  List<Vec2F> particles;
  List<std::shared_ptr<AudioInstance>> sounds;
  std::optional<Vec2F> rejected_particle;
  std::shared_ptr<AudioInstance> rejected_audio;

  std::expected<void, StarException> add_particle(Particle&& particle) override {
    if (accepted.size() == fail_at) {
      fail_at.reset();
      rejected_particle = particle.position;
      return std::unexpected(StarException("Rejected particle"));
    }
    accepted += 'p';
    particles.push_back(particle.position);
    return {};
  }
  std::expected<void, StarException> add_audio(std::shared_ptr<AudioInstance> audio) override {
    if (accepted.size() == fail_at) {
      fail_at.reset();
      rejected_audio = std::move(audio);
      return std::unexpected(StarException("Rejected audio"));
    }
    accepted += 'a';
    sounds.push_back(std::move(audio));
    return {};
  }
};
}

const GameTestRegistration assetsAll("AssetsTest", "All", [](GameTest&) {
  EXPECT_EQ(AssetPath::remove_directives("/foo/bar/baz??::?:??:asdfasdf??D?"), "/foo/bar/baz");
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

static_assert(asset_path_range<StringList> && asset_path_range<CaseInsensitiveStringSet>);
static_assert(asset_path_range<std::array<String, 2>> && !asset_path_range<std::vector<int>>);

const GameTestRegistration assets_atomic_batch_queues("AssetsTest", "AtomicBatchQueues", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory);
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  ASSERT_TRUE(File::write_file(String("[]"), File::relative_to(*directory, "preload.config")));
  // Zero workers: every published ID remains observable, including partial batches.
  auto created = Assets::create(fixture.root().logger(), fixture.root().random(), fixture.root().logMap(),
                                Assets::Settings{}, {*directory});
  ASSERT_TRUE(created) << created.error().what();
  auto& assets = **created;
  ASSERT_TRUE(assets.queue_jsons(StringList{"/one.json:first", "/one.json:second"}));
  ASSERT_TRUE(assets.queue_images(std::array<String, 2>{"/tile.png:frame?flipx", "/tile.png:frame?flipx"}));
  auto audio = CaseInsensitiveString::create("/sound.ogg");
  ASSERT_TRUE(audio);
  ASSERT_TRUE(assets.queue_audios(CaseInsensitiveStringSet{*audio}));
  List<Assets::AssetId> expected{{Assets::AssetType::Json, "/one.json"},
                                 {Assets::AssetType::Image, "/tile.png:frame?flipx"},
                                 {Assets::AssetType::Audio, "/sound.ogg"}};
  ASSERT_EQ(AssetWorkerTestAccess::queued_ids(assets), expected);

  EXPECT_FALSE(assets.queue_jsons(StringList{"/two.json", "/three.json?flipx"}));
  EXPECT_FALSE(assets.queue_images(StringList{"/two.png", "relative.png"}));
  EXPECT_FALSE(assets.queue_audios(StringList{"/two.ogg", "/bad.ogg:sub"}));
  auto good = CaseInsensitiveString::create("/two.ogg");
  auto bad = CaseInsensitiveString::create("/bad.ogg?flipx");
  ASSERT_TRUE(good);
  ASSERT_TRUE(bad);
  EXPECT_FALSE(assets.queue_audios(CaseInsensitiveStringSet{*good, *bad}));
  EXPECT_FALSE(assets.queue_audios(std::array{*good, *bad}));
  EXPECT_EQ(AssetWorkerTestAccess::queued_ids(assets), expected);
});

const GameTestRegistration assets_filtered_scans("AssetsTest", "FilteredScanOrderAndErrors", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory);
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  ASSERT_TRUE(File::write_file(String("[]"), File::relative_to(*directory, "preload.config")));
  for (auto const& name : StringList{"水One.item.JSON", "水Two.item.json", "水Three.ITEM.JSON", "else.item.json"})
    ASSERT_TRUE(File::write_file(String("{}"), File::relative_to(*directory, name)));
  auto created = Assets::create(fixture.root().logger(), fixture.root().random(), fixture.root().logMap(),
                                Assets::Settings{}, {*directory});
  ASSERT_TRUE(created) << created.error().what();
  auto& assets = **created;
  String prefix = "/水t";
  for (auto const& suffix : StringList{".jSoN", ".itEM.json", ""}) {
    // Independent candidate order: the extension index and file map need not agree.
    StringList candidates;
    if (suffix == ".jSoN") {
      auto indexed = assets.scan_extension("json");
      ASSERT_TRUE(indexed);
      for (auto const& key : indexed->get())
        candidates.push_back(key.string());
    } else {
      auto all = assets.scan("");
      ASSERT_TRUE(all);
      candidates = std::move(*all);
    }
    StringList expected;
    for (auto const& path : candidates) {
      auto tail = path.ends_with(suffix, String::CaseInsensitive);
      auto head = path.starts_with(prefix, String::CaseInsensitive);
      ASSERT_TRUE(tail && head);
      if (*tail && *head)
        expected.push_back(path);
    }
    ASSERT_EQ(expected.size(), 2u);
    auto filtered = assets.scan(prefix, suffix);
    ASSERT_TRUE(filtered) << filtered.error().what();
    EXPECT_EQ(*filtered, expected);
  }
  String invalid(std::string_view("\xff", 1));
  EXPECT_FALSE(assets.scan(invalid, ".json"));
  EXPECT_FALSE(assets.scan(invalid, ".item.json"));
  EXPECT_FALSE(assets.scan("/not-present", invalid));
  EXPECT_FALSE(assets.scan("/not-present", "." + invalid));
  for (auto const& suffix : StringList{".unknown", ".item.unknown"}) {
    auto no_candidates = assets.scan(invalid, suffix);
    ASSERT_TRUE(no_candidates);
    EXPECT_TRUE(no_candidates->empty());
  }
});

const GameTestRegistration animation_drawable_transform(
  "AssetsTest", "AnimationImageMatchesCenteredDirectiveComposition", [](GameTest& fixture) {
    auto metadata = fixture.root().image_metadata_database();
    ASSERT_TRUE(metadata) << metadata.error().what();
    AssetPath const image("/humanoid/human/malebody.png?flipx");
    DirectivesGroup const border(String("?border=3;ff0000"));
    Vec2F const offset(2.25f, -7.5f);
    auto color = Color::rgba(15, 92, 173, 201);
    for (bool centered : {false, true}) {
      auto expected = Drawable::make_image(**metadata, image, 0.375f, centered, offset);
      ASSERT_TRUE(expected) << expected.error().what();
      auto applied = expected->image_part().add_directives_group(**metadata, border, true);
      ASSERT_TRUE(applied) << applied.error().what();
      expected->rotate(0.7f);
      expected->color = color;

      auto actual = Drawable::make_animation_image(**metadata, image, 0.375f, centered, offset, border, color);
      ASSERT_TRUE(actual) << actual.error().what();
      actual->rotate(0.7f);
      EXPECT_EQ(*actual, *expected);
    }
  });

const GameTestRegistration metadata_occupancy("AssetsTest", "MetadataOccupancyQueryOrderAndLimits", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory);
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto overlay = File::relative_to(*directory, "assets");
  ASSERT_TRUE(File::create_directory(overlay));
  Image pattern(8, 8, PixelFormat::RGBA32);
  for (unsigned y = 0; y < 8; ++y)
    for (unsigned x = 0; x < 8; ++x)
      ASSERT_TRUE(pattern.set(x, y, Vec4B(7, 11, 3, x <= y ? 1 : 0)));
  auto buffer = std::make_shared<Buffer>();
  ASSERT_TRUE(pattern.write_png(fixture.root().logger(), buffer));
  auto bytes = buffer->take_data();
  ASSERT_TRUE(File::write_file(bytes, File::relative_to(overlay, "pattern.png")));
  ASSERT_TRUE(File::write_file(String("[]"), File::relative_to(overlay, "preload.config")));
  auto settings = fixture.rootSettings();
  settings.assetDirectories.clear();
  settings.assetSources = {overlay};
  settings.storageDirectory = File::relative_to(*directory, "storage");
  settings.runtimeConfigFile = {};
  settings.logFile = {};
  settings.includeUGC = false;
  auto root = Root::create(std::move(settings));
  ASSERT_TRUE(root);
  AssetPath path("/pattern.png");
  auto assets = (*root)->assets();
  ASSERT_TRUE(assets);
  auto pixels = (*assets)->image(path);
  ASSERT_TRUE(pixels) << pixels.error().what();
  ASSERT_EQ((*pixels)->size(), Vec2U(8, 8));
  for (float direction : {-1.f, 1.f}) {
    ImageMetadataDatabase database(**root);
    for (float x : {direction * 0.25f, direction * -0.25f, direction * 0.5f, direction * -0.5f}) {
      Vec2F position(x, -0.5f);
      for (bool flip : {false, true}) {
        for (float limit : {-1.f, 0.f, 1.f / 64, 0.51f, 1.f, 1.25f}) {
          List<Vec2I> expected;
          for (int ty = -1; ty <= 0; ++ty) {
            for (int tx = x < 0 ? -1 : 0; tx <= (x < 0 ? 0 : 1); ++tx) {
              unsigned count = 0;
              for (int dy = 0; dy < 8; ++dy) {
                for (int dx = 0; dx < 8; ++dx) {
                  int px = static_cast<int>(std::round(tx * 8 + dx - x));
                  int py = static_cast<int>(std::round(ty * 8 + dy - position[1]));
                  if (px < 0 || px >= 8 || py < 0 || py >= 8)
                    continue;
                  auto sample = (*pixels)->get(flip ? 7 - px : px, py);
                  ASSERT_TRUE(sample);
                  count += (*sample)[3] != 0;
                }
              }
              if (count / 64.f >= limit)
                expected.emplace_back(tx, ty);
            }
          }
          auto actual = database.image_spaces(path, position, limit, flip);
          ASSERT_TRUE(actual) << actual.error().what();
          EXPECT_EQ(*actual, expected);
        }
      }
    }
    // Materialize incoming IEEE bytes at runtime even in finite-math builds.
    for (volatile std::uint32_t bits : {0x7fc00000u, 0x7f800000u, 0xff800000u}) {
      auto value = std::bit_cast<float>(static_cast<std::uint32_t>(bits));
      EXPECT_FALSE(database.image_spaces(path, {value, 0}, 0, false));
      EXPECT_FALSE(database.image_spaces(path, {0, value}, 0, false));
      EXPECT_FALSE(database.image_spaces(path, {}, value, false));
    }
    for (float value : {-0x1p35f, 0x1p34f})
      EXPECT_FALSE(database.image_spaces(path, {value, 0}, 0, false));
    EXPECT_TRUE(database.image_spaces(path, {-0x1p34f, 0}, 0, false));
  }
});

const GameTestRegistration drawable_directives_retry(
  "DrawableTest", "DirectivesRetryAndUncenteredFastPath", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto overlay = File::relative_to(*directory, "assets");
    ASSERT_TRUE(File::create_directory(overlay));
    auto frames_path = File::relative_to(overlay, "sample.frames");
    String frames(R"({"frameList":{"tile":[0,0,2,2]}})");
    auto png = pngTestBytes(fixture.root().logger());
    ASSERT_TRUE(png);
    ASSERT_TRUE(File::write_file(*png, File::relative_to(overlay, "sample.png")));
    ASSERT_TRUE(File::write_file(*png, File::relative_to(overlay, "patched.png")));
    ASSERT_TRUE(File::write_file(String("function patch(image) return image:subImage({0,0},{1,1}) end"),
                                 File::relative_to(overlay, "patched.png.patch.lua")));
    ASSERT_TRUE(File::write_file(String(R"({"scripts":{"postLoad":["/generate.lua"]}})"), File::relative_to(overlay, "_metadata")));
    ASSERT_TRUE(File::write_file(String("assets.add('/memory.png', assets.image('/sample.png'))"),
                                 File::relative_to(overlay, "generate.lua")));
    ASSERT_TRUE(File::write_file(frames, frames_path));
    ASSERT_TRUE(File::write_file(String("[]"), File::relative_to(overlay, "preload.config")));
    auto settings = fixture.rootSettings();
    settings.assetDirectories.clear();
    settings.assetSources = {overlay};
    settings.storageDirectory = File::relative_to(*directory, "storage");
    settings.runtimeConfigFile = {};
    settings.logFile = {};
    settings.includeUGC = false;
    settings.assetsSettings.missing_image = {};
    auto root = Root::create(std::move(settings));
    ASSERT_TRUE(root) << root.error().what();
    auto assets = (*root)->assets();
    ASSERT_TRUE(assets) << assets.error().what();
    auto database = (*root)->image_metadata_database();
    ASSERT_TRUE(database);
    auto const& metadata = **database;
    for (bool base_first : {false, true}) {
      ImageMetadataDatabase cold(**root);
      for (String const& base : StringList{"/memory.png", "/patched.png"}) {
        auto image = (*assets)->image(base);
        ASSERT_TRUE(image) << image.error().what();
        EXPECT_EQ((*image)->size(), base == "/memory.png" ? Vec2U(2, 2) : Vec2U(1, 1));
        if (base_first)
          EXPECT_EQ(cold.image_size(base), (*image)->size());
        for (String const& suffix : StringList{"?border=2;ffffff", "?scalenearest=0.25", "?scale=0;2", "?scale=-1;2",
                                             "?scalenearest=1000000000?crop=0;0;1;1"}) {
          AssetPath derived(base + suffix);
          auto actual = (*assets)->image(derived);
          ASSERT_TRUE(actual) << actual.error().what();
          EXPECT_EQ(cold.image_size(derived), (*actual)->size());
        }
        EXPECT_EQ(cold.image_size(base), (*image)->size());
      }
    }
    for (String const& suffix : StringList{"?border=4294967295;ffffff", "?border=1000000000;ffffff",
                                         "?scalenearest=1000000000", "?scalebicubic=1000000000?crop=0;0;1;1"}) {
      AssetPath invalid(String("/sample.png") + suffix);
      EXPECT_FALSE((*assets)->image(invalid));
      EXPECT_FALSE(metadata.image_size(invalid));
    }
    auto transform = Mat3F::translation({4, 7}) * Mat3F::scaling({2, -3});
    Drawable::ImagePart const original{"/sample.png:tile?border=1;ffffff", transform};
    Directives border("?border=2;ffffff");
    DirectivesGroup group(String("?flipx"));
    group += border;
    auto single = original, grouped = original, removed = original;

    // Only the original path has a cached size. Every candidate must read frames.
    ASSERT_EQ(metadata.image_size(original.image), Vec2U(4, 4));
    ASSERT_TRUE(File::write_file(String("{"), frames_path));
    (*assets)->hot_reload();
    EXPECT_FALSE(single.add_directives(metadata, border, true));
    EXPECT_FALSE(grouped.add_directives_group(metadata, group, true));
    EXPECT_FALSE(removed.remove_directives(metadata, true));
    EXPECT_EQ(single, original);
    EXPECT_EQ(grouped, original);
    EXPECT_EQ(removed, original);

    ASSERT_TRUE(File::write_file(frames, frames_path));
    (*assets)->hot_reload();
    ASSERT_TRUE(single.add_directives(metadata, border, true));
    ASSERT_TRUE(grouped.add_directives_group(metadata, group, true));
    ASSERT_TRUE(removed.remove_directives(metadata, true));
    EXPECT_EQ(AssetPath::join(single.image), "/sample.png:tile?border=1;ffffff?border=2;ffffff");
    EXPECT_EQ(AssetPath::join(grouped.image), "/sample.png:tile?border=1;ffffff?flipx?border=2;ffffff");
    EXPECT_EQ(AssetPath::join(removed.image), "/sample.png:tile");
    EXPECT_EQ(single.transformation, transform * Mat3F::translation({-2, -2}));
    EXPECT_EQ(grouped.transformation, single.transformation);
    EXPECT_EQ(removed.transformation, transform * Mat3F::translation({1, 1}));

    Drawable::ImagePart missing{"/missing.png", transform};
    ASSERT_FALSE(metadata.image_size(missing.image));
    ASSERT_TRUE(missing.remove_directives(metadata, true));
    ASSERT_TRUE(missing.add_directives(metadata, Directives{}, true));
    ASSERT_TRUE(missing.add_directives_group(metadata, DirectivesGroup{}, true));
    ASSERT_TRUE(missing.add_directives(metadata, border));
    ASSERT_TRUE(missing.add_directives_group(metadata, missing.image.directives));
    EXPECT_EQ(AssetPath::join(missing.image), "/missing.png?border=2;ffffff?border=2;ffffff");
    ASSERT_TRUE(missing.remove_directives(metadata));
    EXPECT_EQ(AssetPath::join(missing.image), "/missing.png");
    EXPECT_EQ(missing.transformation, transform);
  });

const GameTestRegistration drawable_gradient_wire("DrawableTest", "GradientJsonAndLegacyWire", [](GameTest& fixture) {
  auto metadata = fixture.root().image_metadata_database();
  ASSERT_TRUE(metadata);
  auto solid = Drawable::make_line({{1, 2}, {-3, 4}}, 2.5f, Color::rgba(87, 220, 255, 204), {8, -9});
  solid.fullbright = true;
  auto legacy_json = solid.to_json();
  ASSERT_TRUE(legacy_json);
  EXPECT_EQ(legacy_json->contains("endColor"), false);
  EXPECT_EQ(Drawable::create(**metadata, *legacy_json), solid);
  auto gradient = solid;
  gradient.line_part().end_color = Color::rgba(87, 220, 255, 0);
  auto json = gradient.to_json();
  ASSERT_TRUE(json);
  EXPECT_EQ(json->get("endColor"), json_from_color(*gradient.line_part().end_color));
  EXPECT_EQ(Drawable::create(**metadata, *json), gradient);
  auto invalid = json->set("endColor", JsonArray{});
  ASSERT_TRUE(invalid);
  EXPECT_FALSE(Drawable::create(**metadata, *invalid));

  // Independent legacy packet: tag 1, endpoints, width, position, start RGBA, fullbright.
  DataStreamBuffer legacy;
  ASSERT_TRUE(legacy << std::uint8_t{1} << Vec2F(1, 2) << Vec2F(-3, 4) << 2.5f
                     << Vec2F(8, -9) << solid.color.to_rgba_f() << true);
  auto wire = DataStreamBuffer::serialize(gradient);
  ASSERT_TRUE(wire);
  EXPECT_EQ(*wire, legacy.data());
  ASSERT_TRUE(DataStreamBuffer::deserialize(gradient, legacy.span()));
  EXPECT_EQ(gradient, solid);
});

const GameTestRegistration objectCursorPreviewRandomIsolation(
  "AssetsTest", "ObjectCursorPreviewColdAndWarmCachesPreserveGameplayRandom", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
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
    auto files = (*assets)->scan_extension("object");
    ASSERT_TRUE(files) << files.error().what();
    String objectName;
    for (auto const& path : files->get()) {
      auto config = (*assets)->json(path.string());
      ASSERT_TRUE(config) << config.error().what();
      auto flickering = config->contains("flickerPeriod");
      ASSERT_TRUE(flickering);
      if (*flickering) {
        auto name = config->get_string("objectName");
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
    auto closeWorld = make_scope_exit([&] { EXPECT_TRUE((*world)->close()); });
    (*world)->setSpawningEnabled(false);
    Vec2I const position(1024, 1024);
    ASSERT_TRUE((*world)->generateRegion(RectI::with_size(position - Vec2I(64, 64), Vec2I(128, 128))));

    // There is no UniverseServer or world thread. Finish world/database setup
    // before seeding, and use a fresh database so the first preview really
    // parses the installed config, including its randomized light flicker.
    for (auto direction : {Direction::Left, Direction::Right}) {
      auto database = ObjectDatabase::create(root);
      ASSERT_TRUE(database) << database.error().what();
      constexpr std::uint64_t seed = 0x435552534f525052;
      root.random().seed(seed);
      RandomSource untouchedGameplay(seed);
      auto cold = (*database)->cursor_hint_drawables(**world, objectName, position, direction, JsonObject{});
      ASSERT_TRUE(cold) << cold.error().what();
      ASSERT_FALSE(cold->drawables.empty());
      ASSERT_EQ(root.random().next_u64(), untouchedGameplay.next_u64()) << "cold preview";
      auto cachedConfig = (*database)->get_config(objectName);
      ASSERT_TRUE(cachedConfig) << cachedConfig.error().what();
      ASSERT_TRUE((*cachedConfig)->light_flickering);
      ASSERT_EQ(root.random().next_u64(), untouchedGameplay.next_u64()) << "warm config lookup";
      auto warm = (*database)->cursor_hint_drawables(**world, objectName, position, direction, JsonObject{});
      ASSERT_TRUE(warm) << warm.error().what();
      EXPECT_EQ(warm->drawables, cold->drawables);
      EXPECT_EQ(warm->placeable, cold->placeable);
      ASSERT_EQ(root.random().next_u64(), untouchedGameplay.next_u64()) << "warm preview";
    }
  });

const GameTestRegistration object_effective_orientation("ObjectDatabase", "CustomOrientationsDrivePlacementPreviewAndEntity",
    [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory);
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto isolated = effect_test_root(fixture, *directory, {
      {"orientation-effective-open.object", R"({
        "objectName":"orientation-effective-open", "orientations":[
          {"image":"orientation-effective.png","imagePosition":[8,0],"direction":"left"},
          {"image":"orientation-effective.png","imagePosition":[24,0],"direction":"left","anchors":["background"]}
        ]
      })"},
      {"orientation-effective-blocked.object", R"({
        "objectName":"orientation-effective-blocked", "orientations":[
          {"image":"orientation-effective.png","imagePosition":[8,0],"direction":"left","anchors":["background"]},
          {"image":"orientation-effective.png","imagePosition":[24,0],"direction":"left","anchors":["background"]}
        ]
      })"},
      {"orientation-effective.animation", R"({"frames":"orientation-effective.png","variants":4,"frameNumber":1})"}}, true);
  ASSERT_TRUE(isolated) << isolated.error().what();
  auto& root = **isolated;
  auto database = root.object_database();
  ASSERT_TRUE(database) << database.error().what();
  auto storage = File::ephemeral_file(root.random());
  ASSERT_TRUE(storage);
  auto world = WorldServer::create(root, Vec2U(128, 128), *storage);
  ASSERT_TRUE(world) << world.error().what();
  auto close = make_scope_exit([&] { EXPECT_TRUE((*world)->close()); });
  Vec2I position(64, 64);
  constexpr auto direction = Direction::Left;
  String open = "orientation-effective-open";
  String blocked = "orientation-effective-blocked";
  EXPECT_EQ((*database)->can_place_object(**world, position, open), true);
  EXPECT_EQ((*database)->can_place_object(**world, position, blocked), false);

  Json reject = JsonObject{{"customOrientations", JsonArray{JsonObject{{"anchors", JsonArray{"background"}}}}}};
  EXPECT_EQ((*database)->can_place_object(**world, position, open, reject), false);
  auto rejected = (*database)->create_for_placement(**world, open, position, direction, reject);
  ASSERT_TRUE(rejected) << rejected.error().what();
  EXPECT_FALSE(*rejected);
  auto preview = (*database)->cursor_hint_drawables(**world, open, position, direction, reject);
  ASSERT_TRUE(preview) << preview.error().what();
  EXPECT_FALSE(preview->placeable);
  ASSERT_EQ(preview->drawables.size(), 1u);
  EXPECT_EQ(preview->drawables.front().position, Vec2F(position) + Vec2F(3, 0)); // Database: last affinity.
  auto unplaced = (*database)->create_object(open, reject);
  ASSERT_TRUE(unplaced) << unplaced.error().what();
  auto hint = (*unplaced)->cursor_hint_drawables();
  ASSERT_TRUE(hint) << hint.error().what();
  ASSERT_EQ(hint->size(), 1u);
  EXPECT_EQ(hint->front().position, Vec2F(1, 0)); // Unplaced entity: first affinity.

  auto permit = Json::parse(R"({"customOrientations":[{},{
    "anchors":null, "spaces":[[1,0],[2,0]],
    "particleEmitter":{"emissionRate":1,"particle":{"type":"animated","animation":"orientation-effective.animation"}}
  }]})");
  ASSERT_TRUE(permit);
  constexpr std::uint64_t seed = 0x6f7269656e746174;
  root.random().seed(seed);
  RandomSource unchanged(seed);
  EXPECT_EQ((*database)->can_place_object(**world, position, blocked, *permit), true);
  auto permitted = (*database)->cursor_hint_drawables(**world, blocked, position, direction, *permit);
  ASSERT_TRUE(permitted) << permitted.error().what();
  EXPECT_TRUE(permitted->placeable);
  ASSERT_EQ(permitted->drawables.size(), 1u);
  EXPECT_EQ(permitted->drawables.front().position, Vec2F(position) + Vec2F(3, 0));
  EXPECT_EQ(root.random().next_u64(), unchanged.next_u64()) << "Selecting an orientation must not sample its particle variant";

  auto placed = (*database)->create_for_placement(**world, blocked, position, direction, *permit);
  ASSERT_TRUE(placed) << placed.error().what();
  ASSERT_TRUE(*placed);
  ASSERT_TRUE((*placed)->init(**world, 1, EntityMode::Master));
  auto detach = make_scope_exit([&] { EXPECT_TRUE((*placed)->uninit()); });
  List<Vec2I> occupied{{1, 0}, {2, 0}};
  EXPECT_EQ((*placed)->spaces(), occupied);
  ASSERT_TRUE((*placed)->set_tile_position(position + Vec2I(2, 0)));
  EXPECT_EQ((*placed)->spaces(), occupied);
  auto entity_hint = (*placed)->cursor_hint_drawables();
  ASSERT_TRUE(entity_hint) << entity_hint.error().what();
  ASSERT_EQ(entity_hint->size(), 1u);
  EXPECT_EQ(entity_hint->front().position, Vec2F(3, 0));
});

const GameTestRegistration object_catalog_retry("ObjectDatabase", "ConcurrentReadersShareRetriedConfig", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory);
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto isolated = effect_test_root(fixture, *directory, {{"orientation-retry.object", R"({
      "objectName":"orientation-retry", "orientations":[{"imageLayers":[],"frames":0}]
    })"}}, true);
  ASSERT_TRUE(isolated) << isolated.error().what();
  auto& root = **isolated;
  auto database = root.object_database();
  ASSERT_TRUE(database) << database.error().what();
  auto broken = (*database)->get_config("orientation-retry");
  ASSERT_FALSE(broken);
  EXPECT_TRUE(std::string_view(broken.error().what()).contains("frames"));

  auto path = File::relative_to(*directory, "assets/orientation-retry.object");
  ASSERT_TRUE(File::write_file(String(R"({
      "objectName":"orientation-retry", "orientations":[{"imageLayers":[],"frames":3}]
    })"), path));
  auto assets = root.assets();
  ASSERT_TRUE(assets);
  (*assets)->hot_reload();

  using Config = std::expected<std::shared_ptr<ObjectConfig>, StarException>;
  std::latch start(1);
  std::array<std::future<Config>, 8> readers;
  for (auto& reader : readers)
    reader = std::async(std::launch::async, [&] {
      start.wait();
      return (*database)->get_config("orientation-retry");
    });
  start.count_down();
  auto first = readers.front().get();
  ASSERT_TRUE(first) << first.error().what();
  ASSERT_EQ((*first)->orientations.size(), 1u);
  EXPECT_EQ((*first)->orientations.front()->frames, 3);
  for (auto& reader : std::span(readers).subspan(1)) {
    auto result = reader.get();
    ASSERT_TRUE(result) << result.error().what();
    EXPECT_EQ(*result, *first);
  }
  auto cached = (*database)->get_config("orientation-retry");
  ASSERT_TRUE(cached) << cached.error().what();
  EXPECT_EQ(*cached, *first);
});

const GameTestRegistration object_signed_emission_cooldown("ObjectRenderTest", "StockNegativeAndZeroSampledRatesPreserveCooldown",
    [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory);
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto isolated = effect_test_root(fixture, *directory, {{"signed-emission-probe.object", R"({
    "objectName":"signed-emission-probe","hasObjectItem":false,"orientations":[{"imageLayers":[]}]
  })"}}, true);
  ASSERT_TRUE(isolated) << isolated.error().what();
  auto& root = **isolated;
  auto objects = root.object_database();
  ASSERT_TRUE(objects) << objects.error().what();
  auto booster = (*objects)->get_config("boosterflamehuman");
  ASSERT_TRUE(booster) << booster.error().what();
  auto const& emitters = (*booster)->orientations.front()->particle_emitters;
  ASSERT_FALSE(emitters.empty());
  auto const rate = emitters.front().particle_emission_rate;
  auto const variance = emitters.front().particle_emission_rate_variance;
  ASSERT_EQ(rate, 0.4f);
  ASSERT_EQ(variance, 0.5f);
  auto storage = File::ephemeral_file(root.random());
  ASSERT_TRUE(storage);
  auto world = WorldServer::create(root, {128, 128}, *storage);
  ASSERT_TRUE(world) << world.error().what();
  auto close_world = make_scope_exit([&] { EXPECT_TRUE((*world)->close()); });

  // Use the same RNG stream as a real emission: eleven particle variance
  // components, then one rate draw. Pick a well-bounded negative sample.
  std::uint64_t seed = 0;
  float sampled_variance = 0.0f;
  std::uint64_t next_random = 0;
  for (; seed < 1024; ++seed) {
    RandomSource oracle(seed);
    for (int component = 0; component < 11; ++component)
      (void)oracle.uniform_float();
    auto sample = oracle.uniform_float(-variance, variance);
    ASSERT_TRUE(sample) << sample.error().what();
    if (*sample < -0.45f) {
      sampled_variance = *sample;
      next_random = oracle.next_u64();
      break;
    }
  }
  ASSERT_LT(seed, 1024u);

  for (float base_rate : {rate, -sampled_variance}) {
    Json parameters = JsonObject{{"customOrientations", JsonArray{JsonObject{{"particleEmitter", JsonObject{
        {"emissionRate", base_rate}, {"emissionVariance", variance}, {"particle", JsonObject{{"type", "ember"}}}
    }}}}}};
    auto object = (*objects)->create_object("signed-emission-probe", parameters);
    ASSERT_TRUE(object) << object.error().what();
    ASSERT_TRUE((*object)->set_tile_position({64, 64}));
    ASSERT_TRUE((*object)->init(**world, 1, EntityMode::Master));
    auto uninit = make_scope_exit([&] { EXPECT_TRUE((*object)->uninit()); });
    root.random().seed(seed);
    EffectEventSink sink;
    sink.fail_at = 0;
    ASSERT_FALSE((*object)->collect_render_events(sink));
    ASSERT_TRUE((*object)->collect_render_events(sink));
    ASSERT_EQ(sink.particles.size(), 1u);
    EXPECT_EQ(root.random().next_u64(), next_random);

    if (base_rate == rate) {
      float half_cooldown = -0.5f / (rate + sampled_variance);
      ASSERT_TRUE((*object)->update(half_cooldown, 1));
      ASSERT_TRUE((*object)->collect_render_events(sink));
      EXPECT_EQ(sink.particles.size(), 1u);
      ASSERT_TRUE((*object)->update(half_cooldown, 2));
      ASSERT_TRUE((*object)->collect_render_events(sink));
      EXPECT_EQ(sink.particles.size(), 2u);
    } else {
      // Exact cancellation historically produced an infinite cooldown, not a
      // disabled emitter or a retry every tick. Only an orientation reset re-arms it.
      ASSERT_TRUE((*object)->update(1000.0f, 1));
      ASSERT_TRUE((*object)->collect_render_events(sink));
      EXPECT_EQ(sink.particles.size(), 1u);
      ASSERT_TRUE((*object)->update_orientation());
      ASSERT_TRUE((*object)->collect_render_events(sink));
      EXPECT_EQ(sink.particles.size(), 2u);
    }
  }
});

const GameTestRegistration directory_ignore_order(
  "AssetsTest", "DirectoryIgnoreCompilesOnlyReachedCaseInsensitivePartialPatterns", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto branch = File::relative_to(*directory, "branch");
    ASSERT_TRUE(File::create_directory(branch));
    StringList patterns{"^/branch$", "IXE", "[", "("};
    auto build = [&] { return DirectoryAssetSource::create(fixture.root().logger(), *directory, patterns); };
    auto empty = build();
    ASSERT_TRUE(empty) << empty.error().what();
    EXPECT_EQ((*empty)->size(), 0u);

    ASSERT_TRUE(File::write_file(String("x"), File::relative_to(branch, "mixed.txt")));
    auto masked = build();
    ASSERT_TRUE(masked) << masked.error().what();
    EXPECT_EQ((*masked)->size(), 0u);
    // A directory matching the first pattern must still be descended into.
    ASSERT_TRUE(File::write_file(String("y"), File::relative_to(branch, "keep.txt")));
    auto rejected = build();
    ASSERT_FALSE(rejected);
    auto expected_error = String("/branch/keep.txt").regex_match("[", false, false);
    ASSERT_FALSE(expected_error);
    EXPECT_TRUE(rejected.error().is<StringException>());
    EXPECT_EQ(rejected.error().what(), expected_error.error().what());

    // Built-in metadata exclusions run after user patterns, not before them.
    ASSERT_TRUE(File::remove(File::relative_to(branch, "keep.txt")));
    ASSERT_TRUE(File::write_file(String("{}"), File::relative_to(*directory, "_metadata")));
    auto metadata_rejected = build();
    ASSERT_FALSE(metadata_rejected);
    EXPECT_EQ(metadata_rejected.error().what(), expected_error.error().what());
  });

const GameTestRegistration directory_index_and_metadata(
  "AssetsTest", "DirectoryBorrowsSortedPathsAndUpdatesPreferredMetadataFile", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto preferred = File::relative_to(*directory, "_metadata");
    auto fallback = File::relative_to(*directory, ".metadata");
    ASSERT_TRUE(File::write_file(String(R"({"version":1})"), preferred));
    ASSERT_TRUE(File::write_file(String(R"({"version":0})"), fallback));
    ASSERT_TRUE(File::write_file(String("last"), File::relative_to(*directory, "z")));
    ASSERT_TRUE(File::write_file(String("first"), File::relative_to(*directory, "a")));
    auto created = DirectoryAssetSource::create(fixture.root().logger(), *directory);
    ASSERT_TRUE(created) << created.error().what();
    auto& source = **created;
    AssetSource const& base = source;
    static_assert(std::same_as<decltype(base.metadata()), JsonObject const&>);
    static_assert(std::same_as<decltype(source.path_view()), std::span<String const>>);

    auto const& metadata = base.metadata();
    EXPECT_EQ(metadata, (JsonObject{{"version", 1}}));
    auto paths = source.path_view();
    ASSERT_EQ(base.size(), 2u);
    ASSERT_EQ(paths.size(), base.size());
    EXPECT_EQ(paths[0], "/a");
    EXPECT_EQ(paths[1], "/z");
    auto snapshot = base.asset_paths();
    ASSERT_EQ(snapshot, (StringList{"/a", "/z"}));
    snapshot.clear();
    std::size_t visited = 0;
    auto visit = base.for_each_path([&](String const& path) -> std::expected<void, StarException> {
      EXPECT_LT(visited, paths.size());
      if (visited < paths.size())
        EXPECT_EQ(std::addressof(path), std::addressof(paths[visited]));
      ++visited;
      return {};
    });
    ASSERT_TRUE(visit) << visit.error().what();
    EXPECT_EQ(visited, paths.size());

    ASSERT_TRUE(source.set_metadata({{"version", 2}}));
    EXPECT_EQ(metadata, (JsonObject{{"version", 2}}));
    auto updated = DirectoryAssetSource::create(fixture.root().logger(), *directory);
    ASSERT_TRUE(updated) << updated.error().what();
    EXPECT_EQ((*updated)->metadata(), metadata);
    ASSERT_TRUE(source.set_metadata({}));
    auto remaining = DirectoryAssetSource::create(fixture.root().logger(), *directory);
    ASSERT_TRUE(remaining) << remaining.error().what();
    EXPECT_EQ((*remaining)->metadata(), (JsonObject{{"version", 0}}));
    EXPECT_EQ(source.missing_file_path("/_metadata"), std::optional<String>(preferred));
    EXPECT_FALSE(source.missing_file_path("/.metadata"));
    EXPECT_FALSE(source.missing_file_path("relative"));
  });

const GameTestRegistration directory_ancestor_warnings(
  "AssetsTest", "DirectoryCyclesWarnForEachLexicalAliasAndPropagateLoggingFailure", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto real = File::relative_to(*directory, "real");
    auto deep = File::relative_to(real, "deep");
    ASSERT_TRUE(File::create_directory(real));
    ASSERT_TRUE(File::create_directory(deep));
    ASSERT_TRUE(File::write_file(String("payload"), File::relative_to(deep, "leaf")));
    std::error_code ec;
    std::filesystem::create_directory_symlink(fsPath(real), fsPath(File::relative_to(deep, "back")), ec);
    if (ec)
      GTEST_SKIP() << "cannot create directory symlinks here: " << ec.message();
    std::filesystem::create_directory_symlink(fsPath(real), fsPath(File::relative_to(*directory, "alias")), ec);
    ASSERT_FALSE(ec) << ec.message();

    struct Warnings : LogSink {
      std::expected<void, StarException> log(std::string_view message, LogLevel level) override {
        EXPECT_EQ(level, LogLevel::Warn);
        messages.emplace_back(message);
        if (fail)
          return std::unexpected(IOException("Directory warning could not be written"));
        return {};
      }
      std::vector<std::string> messages;
      bool fail = false;
    };
    Logger logger;
    logger.remove_stdout_sink();
    auto warnings = std::make_shared<Warnings>();
    logger.add_sink(warnings);
    auto source = DirectoryAssetSource::create(logger, *directory);
    ASSERT_TRUE(source) << source.error().what();
    EXPECT_EQ((*source)->asset_paths(), (StringList{"/alias/deep/leaf", "/real/deep/leaf"}));
    ASSERT_EQ(warnings->messages.size(), 2u);
    for (std::string_view path : {"/alias/deep/back", "/real/deep/back"})
      EXPECT_EQ(std::ranges::count_if(warnings->messages, [&](auto const& warning) { return warning.contains(path); }), 1);
    warnings->messages.clear();
    warnings->fail = true;
    auto failed = DirectoryAssetSource::create(logger, *directory);
    ASSERT_FALSE(failed);
    EXPECT_TRUE(failed.error().is<IOException>());
    EXPECT_EQ(warnings->messages.size(), 1u);
  });

const GameTestRegistration assetsSymlinkLoopIsNotFollowed("AssetsTest", "SymlinkLoopIsNotFollowed", [](GameTest& fixture) {
  // A link back to an ancestor must not make asset scanning fail with ELOOP.
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory);
  auto tempDir = std::move(*directory);
  auto finallyGuard = make_scope_exit([&tempDir]() { EXPECT_TRUE(File::remove_all(tempDir)); });

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
  auto paths = (*source)->asset_paths();
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
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
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
    EXPECT_EQ((*source)->read("/unindexed.txt"), ByteArray::from_string("not enumerated"));
    std::array<String, 7> requests{"/mutable.json",    "/missing", "/mutable.json", "/MUTABLE.json",
                                   "/../mutable.json", "/",        "/unindexed.txt"};
    expect_batch_matches_scalar(**source, requests);

    Assets::Settings settings{};
    settings.worker_pool_size = 1;
    auto assets =
      Assets::create(fixture.root().logger(), fixture.root().random(), fixture.root().logMap(), settings, {*directory});
    ASSERT_TRUE(assets) << assets.error().what();
    ASSERT_TRUE((*assets)->json("/mutable.json"));
    String replacement(R"({"recovered":42})");
    ASSERT_TRUE(File::write_file(replacement, path));
    (*assets)->hot_reload();
    size = (*source)->file_size("/mutable.json");
    ASSERT_TRUE(size);
    EXPECT_EQ(*size, static_cast<StreamOffset>(replacement.utf8_size()));
    expect_batch_matches_scalar(**source, requests);
    auto recovered = (*assets)->json("/mutable.json");
    ASSERT_TRUE(recovered);
    EXPECT_EQ(recovered->get_int("recovered"), 42);

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
    (*assets)->hot_reload();
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
    auto snapshot = memory.asset_paths();
    std::size_t visited = 0;
    auto stop_after_first = [&, token = std::make_unique<int>(7)](String const& path) -> std::expected<void, StarException> {
      EXPECT_EQ(path, snapshot.front());
      EXPECT_EQ(*token, 7);
      ++visited;
      return std::unexpected(AssetSourceException("stop enumerating"));
    };
    auto stopped = memory.for_each_path(stop_after_first);
    ASSERT_FALSE(stopped);
    EXPECT_TRUE(stopped.error().is<AssetSourceException>());
    EXPECT_EQ(visited, 1u);
    for (auto const& asset_path : snapshot) {
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

const GameTestRegistration packed_streamed_build(
  "AssetsTest", "PackedBuildStreamsExactBytesInProgressOrderBeforeReplacement", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto input = File::relative_to(*directory, "input");
    ASSERT_TRUE(File::create_directory(input));
    ByteArray large;
    large.resize(145123);
    for (std::size_t i = 0; i < large.size(); ++i)
      large.span()[i] = static_cast<char>(i % 251);
    auto first = File::relative_to(input, "Mixed.TXT");
    auto second = File::relative_to(input, "A.bin");
    ASSERT_TRUE(File::write_file(large, first));
    ASSERT_TRUE(File::write_file(std::span("a\0b", 3), second));
    ASSERT_TRUE(File::write_file(ByteArray{}, File::relative_to(input, "empty")));
    ASSERT_TRUE(File::write_file(String(R"({"name":"archive"})"), File::relative_to(input, "_metadata")));
    auto source = DirectoryAssetSource::create(fixture.root().logger(), input);
    ASSERT_TRUE(source);
    auto target = File::relative_to(*directory, "target.pak");
    ASSERT_TRUE(File::write_file(String("previous"), target));
    StringList visited;
    auto progress = [&](std::size_t i, std::size_t total, String const& filename, String const& path)
        -> std::expected<void, StarException> {
      EXPECT_EQ(i, visited.size());
      EXPECT_EQ(total, 3u);
      visited.push_back(path);
      EXPECT_EQ((*source)->to_filesystem(path), filename);
      EXPECT_EQ(File::read_file_string(target), String("previous"));
      if (i == 0)
        return File::write_file(String("changed after capture"), filename);
      return {};
    };
    ASSERT_TRUE(PackedAssetSource::build(**source, target, {"TXT", "txt", "bIN"}, progress));
    EXPECT_EQ(visited, (StringList{"/Mixed.TXT", "/A.bin", "/empty"}));

    // Independent construction of the legacy header, raw payloads and map.
    DataStreamBuffer expected;
    std::uint64_t next = 16 + large.size();
    ASSERT_TRUE(expected.write_data(std::span("SBAsset6", 8)));
    ASSERT_TRUE(expected.write(next + 3));
    ASSERT_TRUE(expected.write_data(large.span()));
    ASSERT_TRUE(expected.write_data(std::span("a\0b", 3)));
    ASSERT_TRUE(expected.write_data(std::span("INDEX", 5)));
    ASSERT_TRUE(expected.write((*source)->metadata()));
    StringMap<std::pair<std::uint64_t, std::uint64_t>> index;
    ASSERT_TRUE(index.add("/Mixed.TXT", {16, large.size()}));
    ASSERT_TRUE(index.add("/A.bin", {next, 3}));
    ASSERT_TRUE(index.add("/empty", {next + 3, 0}));
    ASSERT_TRUE(expected.write(index));
    EXPECT_EQ(File::read_file(target), expected.data());
    auto packed = PackedAssetSource::create(target);
    ASSERT_TRUE(packed);
    EXPECT_EQ((*packed)->read("/Mixed.TXT"), large);
    EXPECT_EQ((*packed)->read("/empty"), ByteArray{});
    StringList enumerated;
    ASSERT_TRUE((*packed)->for_each_path([&](String const& path) -> std::expected<void, StarException> {
      enumerated.push_back(path);
      return {};
    }));
    EXPECT_EQ(enumerated, (*packed)->asset_paths());
    EXPECT_EQ((*packed)->size(), enumerated.size());

    auto reader = (*packed)->open("/Mixed.TXT");
    ASSERT_TRUE(reader);
    ASSERT_TRUE((*reader)->seek(2, IOSeek::End));
    auto start = static_cast<StreamOffset>(large.size() - 2);
    EXPECT_EQ((*reader)->pos(), start);
    EXPECT_FALSE((*reader)->seek(-1));
    EXPECT_EQ((*reader)->pos(), start);
    auto writable = File::open(target, IOMode::ReadWrite);
    ASSERT_TRUE(writable);
    ASSERT_TRUE((*writable)->resize(next - 1));
    std::array<char, 2> tail;
    EXPECT_FALSE((*reader)->read(tail));
    EXPECT_EQ((*reader)->pos(), start);
    ASSERT_TRUE((*writable)->close());
  });

const GameTestRegistration packed_build_rollback(
  "AssetsTest", "PackedBuildDiscardsOnlyItsStageOnCallbackAndInputErrors", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto input = File::relative_to(*directory, "input");
    ASSERT_TRUE(File::create_directory(input));
    auto a = File::relative_to(input, "a");
    auto b = File::relative_to(input, "b");
    ASSERT_TRUE(File::write_file(String("old"), a));
    ASSERT_TRUE(File::write_file(String("two"), b));
    auto source = DirectoryAssetSource::create(fixture.root().logger(), input);
    ASSERT_TRUE(source);
    auto target = File::relative_to(*directory, "target.pak");
    auto stage = target + ".new";
    ASSERT_TRUE(PackedAssetSource::build(**source, target));
    auto original = File::read_file(target);
    ASSERT_TRUE(original);
    ASSERT_TRUE(File::write_file(String("replacement"), a));
    std::size_t calls = 0;
    auto stop = [&](std::size_t, std::size_t, String const&, String const& path) -> std::expected<void, StarException> {
      ++calls;
      EXPECT_EQ(path, "/a");
      return std::unexpected(AssetSourceException("stop packing"));
    };
    auto failed = PackedAssetSource::build(**source, target, {}, stop);
    ASSERT_FALSE(failed);
    EXPECT_TRUE(failed.error().is<AssetSourceException>());
    EXPECT_EQ(calls, 1u);
    EXPECT_EQ(File::read_file(target), *original);
    EXPECT_EQ(File::exists(stage), false);

    ASSERT_TRUE(File::write_file(String("not ours"), stage));
    EXPECT_FALSE(PackedAssetSource::build(**source, target, {}, stop));
    EXPECT_EQ(calls, 1u);
    EXPECT_EQ(File::read_file_string(stage), String("not ours"));
    EXPECT_EQ(File::read_file(target), *original);
    ASSERT_TRUE(File::remove(stage));

    ASSERT_TRUE(File::remove(b));
    EXPECT_FALSE(PackedAssetSource::build(**source, target));
    EXPECT_EQ(File::read_file(target), *original);
    EXPECT_EQ(File::exists(stage), false);
    ASSERT_TRUE(File::remove(target));
    EXPECT_FALSE(PackedAssetSource::build(**source, target));
    EXPECT_EQ(File::exists(target), false);
    EXPECT_EQ(File::exists(stage), false);
  });

const GameTestRegistration packedMetadataLengths(
  "AssetsTest", "PackedMetadataLengthsValidateFreshExtentAndMalformedEntries", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
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
    ASSERT_EQ(before_truncation.size(), duplicate_requests.size());
    ASSERT_TRUE(before_truncation[0]);
    ASSERT_TRUE(before_truncation[1]);
    auto payload = (*packed)->read("/binary");
    ASSERT_TRUE(payload);
    EXPECT_EQ(*payload, ByteArray(std::span("one\0two", 7)));
    auto stream = (*packed)->open("/binary");
    ASSERT_TRUE(stream);
    ASSERT_TRUE((*stream)->seek(2));
    auto cloned = (*stream)->clone();
    ASSERT_TRUE(cloned);
    EXPECT_EQ((*packed)->read("/binary"), *payload);
    EXPECT_EQ((*stream)->pos(), 2);
    EXPECT_EQ((*cloned)->pos(), 2);
    EXPECT_EQ((*stream)->read_bytes(5), ByteArray(std::span("e\0two", 5)));
    EXPECT_EQ((*cloned)->pos(), 2);
    EXPECT_EQ((*cloned)->read_bytes(5), ByteArray(std::span("e\0two", 5)));
    auto writable = File::open(packed_path, IOMode::ReadWrite);
    ASSERT_TRUE(writable);
    ASSERT_TRUE((*writable)->resize(22));// Asset starts at 16 and needs seven bytes.
    auto truncated = (*packed)->file_size("/binary");
    ASSERT_FALSE(truncated);
    EXPECT_TRUE(truncated.error().is<AssetSourceException>());
    expect_batch_matches_scalar(**packed, requests);
    auto during_truncation = (*packed)->file_sizes(duplicate_requests);
    ASSERT_EQ(during_truncation.size(), duplicate_requests.size());
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
    ASSERT_EQ(after_recovery.size(), duplicate_requests.size());
    ASSERT_TRUE(after_recovery[0]);
    ASSERT_TRUE(after_recovery[1]);
    EXPECT_EQ(*after_recovery[0], 7);
    EXPECT_EQ(*after_recovery[1], 7);

    for (auto const& entry :
         std::array{std::pair<std::uint64_t, std::uint64_t>{std::numeric_limits<std::uint64_t>::max(), 1},
                    std::pair<std::uint64_t, std::uint64_t>{16, std::numeric_limits<std::uint64_t>::max()}}) {
      auto malformed_path = File::relative_to(*directory, "malformed.pak");
      DataStreamBuffer malformed;
      ASSERT_TRUE(malformed.write_data(std::span("SBAsset6", 8)));
      ASSERT_TRUE(malformed.write(std::uint64_t(16)));
      ASSERT_TRUE(malformed.write_data(std::span("INDEX", 5)));
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

const GameTestRegistration
  packedEmptyReadExtent("AssetsTest", "PackedEmptyReadsValidateFreshExtentBeforeReturning", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto path = File::relative_to(*directory, "empty.pak");
    DataStreamBuffer bytes;
    ASSERT_TRUE(bytes.write_data(std::span("SBAsset6", 8)));
    ASSERT_TRUE(bytes.write(std::uint64_t(16)));
    ASSERT_TRUE(bytes.write_data(std::span("INDEX", 5)));
    ASSERT_TRUE(bytes.write(JsonObject{}));
    ASSERT_TRUE(
      bytes.write(StringMap<std::pair<std::uint64_t, std::uint64_t>>{{"/empty", {16, 0}}, {"/past", {17, 0}}}));
    ASSERT_TRUE(File::write_file(bytes.data(), path));
    auto source = PackedAssetSource::create(path);
    ASSERT_TRUE(source);
    auto writable = File::open(path, IOMode::ReadWrite);
    ASSERT_TRUE(writable);
    ASSERT_TRUE((*writable)->resize(16));
    EXPECT_EQ((*source)->read("/empty"), ByteArray{});
    auto past = (*source)->read("/past");
    ASSERT_FALSE(past);
    EXPECT_TRUE(past.error().is<AssetSourceException>());
    EXPECT_NE(std::string_view(past.error().what()).find("out of range size"), std::string_view::npos);
    ASSERT_TRUE((*writable)->resize(0));
    auto empty = (*source)->read("/empty");
    ASSERT_FALSE(empty);
    EXPECT_TRUE(empty.error().is<AssetSourceException>());
    auto missing = (*source)->read("/missing");
    ASSERT_FALSE(missing);
    EXPECT_TRUE(missing.error().is<AssetSourceException>());
    EXPECT_NE(std::string_view(missing.error().what()).find("does not exist"), std::string_view::npos);
    ASSERT_TRUE((*writable)->close());
  });

#ifdef STAR_SYSTEM_LINUX
const GameTestRegistration
  directoryReadMetadataExtent("AssetsTest", "DirectoryReadsUseMetadataExtentRatherThanReadingToEof",
                              [](GameTest& fixture) {
                                auto directory = File::temporary_directory(fixture.root().random());
                                ASSERT_TRUE(directory);
                                auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
                                auto path = File::relative_to(*directory, "command-line");
                                std::error_code ec;
                                std::filesystem::create_symlink("/proc/self/cmdline", fsPath(path), ec);
                                ASSERT_FALSE(ec) << ec.message();
                                auto source = DirectoryAssetSource::create(fixture.root().logger(), *directory);
                                ASSERT_TRUE(source);
                                EXPECT_EQ((*source)->read("/command-line"), ByteArray{});
                                auto whole = File::read_file(path);
                                ASSERT_TRUE(whole);
                                EXPECT_FALSE(whole->empty());
                              });
#endif

const GameTestRegistration assetMetadataDigest(
  "AssetsTest", "MetadataDigestMatchesFormerReaderBytesForDirectoryAndPackedSources", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
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
      settings.worker_pool_size = 1;
      settings.digest_ignore = {"^/ignored\\.txt$"};
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

const GameTestRegistration digest_ignore_pattern_order(
  "AssetsTest", "DigestIgnoreCompilesOnlyReachedCaseInsensitivePartialPatterns", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    ASSERT_TRUE(File::write_file(String("[]"), File::relative_to(*directory, "preload.config")));
    ASSERT_TRUE(File::write_file(String("hello"), File::relative_to(*directory, "mixed.txt")));
    Assets::Settings settings{};
    settings.worker_pool_size = 1;
    settings.digest_ignore = {"IXE", "LOAD", "[", "("};
    auto build = [&] {
      return Assets::create(fixture.root().logger(), fixture.root().random(), fixture.root().logMap(), settings,
                            {*directory});
    };
    {
      auto assets = build();
      ASSERT_TRUE(assets) << assets.error().what();
      EXPECT_EQ((*assets)->digest(), Sha256Hasher{}.compute());
    }

    // Only the new final path reaches an invalid pattern. Report the first one.
    ASSERT_TRUE(File::write_file(String("extra"), File::relative_to(*directory, "zzz.txt")));
    auto rejected = build();
    ASSERT_FALSE(rejected);
    auto first_error = String("/zzz.txt").regex_match("[", false, false);
    ASSERT_FALSE(first_error);
    EXPECT_TRUE(rejected.error().is<StringException>());
    EXPECT_EQ(rejected.error().what(), first_error.error().what());

    settings.digest_ignore = {"IXE", "LOAD"};
    auto assets = build();
    ASSERT_TRUE(assets) << assets.error().what();
    auto historical = AssetWorkerTestAccess::reader_digest(**assets);
    ASSERT_TRUE(historical) << historical.error().what();
    EXPECT_EQ((*assets)->digest(), *historical);
    EXPECT_NE((*assets)->digest(), Sha256Hasher{}.compute());
  });

const GameTestRegistration assetBatchDigestPatchProviders(
  "AssetsTest", "BatchDigestMatchesReaderOracleWithInlineAndMemoryPatchProviders", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
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
      settings.worker_pool_size = 1;
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
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    using Index = StringMap<std::pair<std::uint64_t, std::uint64_t>>;
    auto write_packed = [](String const& path, Index const& index) -> std::expected<void, StarException> {
      DataStreamBuffer bytes;
      if (auto result = bytes.write_data(std::span("SBAsset6", 8)); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = bytes.write(std::uint64_t(18)); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = bytes.write_data(std::span("[]", 2)); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = bytes.write_data(std::span("INDEX", 5)); !result)
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
      settings.worker_pool_size = 1;
      auto assets = Assets::create(fixture.root().logger(), fixture.root().random(), fixture.root().logMap(), settings,
                                   {first_path, second_path});
      ASSERT_FALSE(assets);
      EXPECT_EQ(assets.error().what(), expected_error.error().what());
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
  auto partResult = frozen.active_part("body");
  ASSERT_TRUE(partResult);
  auto const& part = partResult->get();
  ASSERT_TRUE(part.active_state);
  EXPECT_EQ(part.active_state->frame, 1u);
  auto partFrame = part.properties.get("partFrame");
  ASSERT_TRUE(partFrame);
  auto partFrameValue = partFrame->get().to_int();
  ASSERT_TRUE(partFrameValue);
  EXPECT_EQ(*partFrameValue, 40);
  auto nextPartFrame = part.next_properties.get("partFrame");
  ASSERT_TRUE(nextPartFrame);
  auto nextPartFrameValue = nextPartFrame->get().to_int();
  ASSERT_TRUE(nextPartFrameValue);
  EXPECT_EQ(*nextPartFrameValue, 30);
  auto stateResult = frozen.active_state("movement");
  ASSERT_TRUE(stateResult);
  auto const& state = stateResult->get();
  EXPECT_EQ(state.frame, 1u);
  EXPECT_EQ(state.next_frame, 0u);
  EXPECT_FLOAT_EQ(state.frame_progress, 0.5f);
  auto stateFrame = state.properties.get("stateFrame");
  ASSERT_TRUE(stateFrame);
  auto stateFrameValue = stateFrame->get().to_int();
  ASSERT_TRUE(stateFrameValue);
  EXPECT_EQ(*stateFrameValue, 20);

  AnimatedPartSet const callbackCopy = animation;
  unsigned stateCount = 0;
  ASSERT_TRUE(callbackCopy.for_each_active_state([&](String const& name, auto const& active) -> std::expected<void, StarException> {
    EXPECT_EQ(name, "movement");
    EXPECT_EQ(active.frame, 1u);
    ++stateCount;
    return {};
  }));
  unsigned partCount = 0;
  ASSERT_TRUE(callbackCopy.for_each_active_part([&](String const& name, auto const& active) -> std::expected<void, StarException> {
    EXPECT_EQ(name, "body");
    auto frame = active.properties.get("partFrame");
    if (!frame)
      return std::unexpected(std::move(frame.error()));
    auto value = frame->get().to_int();
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
  writablePart->get().part_properties["base"] = false;
  writablePart->get().active_part_dirty = true;
  auto updatedPart = animation.active_part("body");
  ASSERT_TRUE(updatedPart);
  auto base = updatedPart->get().properties.get("base");
  ASSERT_TRUE(base);
  auto baseValue = base->get().to_bool();
  ASSERT_TRUE(baseValue);
  EXPECT_FALSE(*baseValue);
  ASSERT_TRUE(animation.set_state_type_enabled("movement", false));
  auto disabledPart = animation.active_part("body");
  ASSERT_TRUE(disabledPart);
  EXPECT_FALSE(disabledPart->get().active_state);
});

const GameTestRegistration
  assetsAnimationTransformCenters("AssetsTest", "TransformCentersPreserveValidationAndRecovery", [](GameTest&) {
    auto config = Json::parse(R"({"parts":{"body":{"properties":{"transforms":[["reset"],["scale",[2,3]]]}}}})");
    ASSERT_TRUE(config);
    auto animation = AnimatedPartSet::create(*config, 1);
    ASSERT_TRUE(animation);
    auto part = animation->active_part("body");
    ASSERT_TRUE(part);
    EXPECT_FLOAT_EQ(part->get().x_scale_animation, 2);
    EXPECT_FLOAT_EQ(part->get().y_scale_animation, 3);
    EXPECT_FLOAT_EQ(part->get().x_translation_animation, 0);
    EXPECT_FLOAT_EQ(part->get().y_translation_animation, 0);
    auto definition = animation->parts().get("body");
    ASSERT_TRUE(definition);
    definition->get().part_properties["transforms"] =
      JsonArray{JsonArray{"reset"}, JsonArray{"scale", JsonArray{2, 3}, JsonArray{1, 2}}};
    definition->get().part_properties["scalingCenter"] = JsonArray{"unused invalid coordinate"};
    definition->get().active_part_dirty = true;
    auto explicitCenter = animation->active_part("body");
    ASSERT_TRUE(explicitCenter);
    EXPECT_FLOAT_EQ(explicitCenter->get().x_translation_animation, -1);
    EXPECT_FLOAT_EQ(explicitCenter->get().y_translation_animation, -4);
    // A supplied default's array type is still checked before using an explicit center.
    definition->get().part_properties["scalingCenter"] = 7;
    definition->get().active_part_dirty = true;
    auto invalidDefault = animation->active_part("body");
    ASSERT_FALSE(invalidDefault);
    EXPECT_TRUE(invalidDefault.error().is<JsonException>());
    definition->get().part_properties["scalingCenter"] = JsonArray{0, 0};
    definition->get().active_part_dirty = true;
    auto recovered = animation->active_part("body");
    ASSERT_TRUE(recovered);
    EXPECT_FLOAT_EQ(recovered->get().x_translation_animation, -1);
    EXPECT_FLOAT_EQ(recovered->get().y_translation_animation, -4);
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
      auto result = animation->active_part("body", sample);
      ASSERT_TRUE(result);
      auto const& part = result->get();
      ASSERT_TRUE(part.active_state);
      EXPECT_EQ(part.active_state->frame, frame);
      auto currentValue = part.properties.get("value");
      auto nextValue = part.next_properties.get("value");
      ASSERT_TRUE(currentValue);
      ASSERT_TRUE(nextValue);
      EXPECT_EQ(currentValue->get(), Json(value));
      EXPECT_EQ(nextValue->get(), Json(next));
      EXPECT_NEAR(part.x_translation_animation, translation, 0.0001f);
      auto base = part.properties.get("baseOnly");
      ASSERT_TRUE(base);
      EXPECT_EQ(base->get(), Json(true));
    };
    animation->prepare_sample(early, 0.25f);
    animation->prepare_sample(late, 0.75f);
    check(early, 0, "part0", "part1", 2);
    check(late, 0, "part0", "part1", 6);
    check(early, 0, "part0", "part1", 2);
    animation->prepare_sample(early, 0.5f);
    check(early, 0, "part0", "part1", 4);
    auto state = animation->active_state("movement", early);
    ASSERT_TRUE(state);
    auto stateValue = state->get().properties.get("value");
    auto nextStateValue = state->get().next_properties.get("value");
    ASSERT_TRUE(stateValue);
    ASSERT_TRUE(nextStateValue);
    EXPECT_EQ(stateValue->get(), Json("state0"));
    EXPECT_EQ(nextStateValue->get(), Json("state1"));
    auto typeOnly = state->get().properties.get("typeOnly");
    ASSERT_TRUE(typeOnly);
    EXPECT_EQ(typeOnly->get(), Json(true));

    ASSERT_TRUE(animation->set_presentation_state("movement", "idle", false, false, early));
    check(early, 0, "idlePart", "idlePart", 0);
    check(late, 0, "part0", "part1", 6);
    auto logical = animation->active_state("movement");
    ASSERT_TRUE(logical);
    EXPECT_EQ(logical->get().state_name, "walk");
    animation->prepare_sample(early, 0.5f);
    check(early, 0, "part0", "part1", 4);

    ASSERT_TRUE(animation->update(0.4f));
    animation->prepare_sample(early, 0.5f);
    check(early, 1, "part1", "part0", 8);
    ASSERT_TRUE(animation->set_active_state("movement", "walk", true, true));
    ASSERT_TRUE(animation->update(0.4f));
    animation->prepare_sample(early, 0.25f);
    check(early, 1, "part1", "part0", 8);
    animation->prepare_sample(early, 0.75f);
    check(early, 1, "part1", "part0", 4);
    auto reversed = animation->active_part("body", early);
    ASSERT_TRUE(reversed);
    EXPECT_TRUE(reversed->get().active_state->reverse);

    ASSERT_TRUE(animation->set_state_type_enabled("overlay", true));
    animation->prepare_sample(early, 0.5f);
    check(early, 0, "overlay", "overlay", 0);
    ASSERT_TRUE(animation->set_presentation_state("overlay", "invalid", false, false, early));
    auto invalid = animation->active_part("body", early);
    ASSERT_FALSE(invalid);
    EXPECT_TRUE(invalid.error().is<JsonException>());
    // Preparing again discards the local override, including its failed cache.
    animation->prepare_sample(early, 0.5f);
    check(early, 0, "overlay", "overlay", 0);
    ASSERT_TRUE(animation->set_state_type_enabled("overlay", false));
    animation->prepare_sample(early, 0.5f);
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
      owner.prepare_sample(sample, 0.5f);
      auto result = owner.active_part("body", sample);
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
    definition->get().part_properties["value"] = 2;
    definition->get().active_part_dirty = true;
    check(*animation, 2);
    // No further parts() call: a reference escaped before the previous prepare.
    definition->get().part_properties["value"] = 3;
    definition->get().part_properties["scalingCenter"] = 7;
    definition->get().active_part_dirty = true;
    animation->prepare_sample(sample, 0.5f);
    auto invalid = animation->active_part("body", sample);
    ASSERT_FALSE(invalid);
    EXPECT_TRUE(invalid.error().is<JsonException>());
    definition->get().part_properties["scalingCenter"] = JsonArray{0, 0};
    definition->get().active_part_dirty = true;
    // A failed query remains retryable even without preparing again.
    auto recovered = animation->active_part("body", sample);
    ASSERT_TRUE(recovered);
    auto recoveredValue = recovered->get().properties.get("value");
    ASSERT_TRUE(recoveredValue);
    EXPECT_EQ(recoveredValue->get(), Json(3));
    EXPECT_FLOAT_EQ(recovered->get().x_scale_animation, 2);
    check(*animation, 3);
    AnimatedPartSet copy = *animation;
    check(copy, 3);
    definition->get().part_properties["value"] = 4;
    definition->get().active_part_dirty = true;
    check(*animation, 4);
    check(copy, 3);

    // Assignment must not forget the escaped map reference in the destination,
    // even when its source has never exposed mutable definitions.
    *animation = *other;
    check(*animation, 9);
    auto replacement = retainedParts.get("body");
    ASSERT_TRUE(replacement);
    replacement->get().part_properties["value"] = 10;
    replacement->get().active_part_dirty = true;
    check(*animation, 10);
    auto fresh = AnimatedPartSet::create(*otherConfig, 1);
    ASSERT_TRUE(fresh);
    *animation = std::move(*fresh);
    check(*animation, 9);
    replacement = retainedParts.get("body");
    ASSERT_TRUE(replacement);
    replacement->get().part_properties["value"] = 11;
    replacement->get().active_part_dirty = true;
    check(*animation, 11);
    check(*other, 9);
  });

const GameTestRegistration assets_animation_enabled_names("AssetsTest", "AnimationEnabledNamesAreAtomic", [](GameTest&) {
  auto config = Json::parse(R"({"stateTypes":{
    "base":{"default":"on","states":{"on":{}}},
    "overlay":{"enabled":false,"default":"on","states":{"on":{}}}
  }})");
  ASSERT_TRUE(config);
  auto set = AnimatedPartSet::create(*config, 1);
  ASSERT_TRUE(set);
  EXPECT_FALSE(set->set_enabled_state_types({"overlay", "missing"}));
  auto base = set->state_type_enabled("base");
  auto overlay = set->state_type_enabled("overlay");
  ASSERT_TRUE(base);
  ASSERT_TRUE(overlay);
  EXPECT_TRUE(*base);
  EXPECT_FALSE(*overlay);
});

const GameTestRegistration assets_animation_transition_ring("AssetsTest", "AnimationFinishRejectsTransitionRing", [](GameTest&) {
  auto config = Json::parse(R"({"stateTypes":{"movement":{"default":"start","states":{
    "start":{"mode":"transition","transition":"a"},"a":{"mode":"transition","transition":"b"},
    "b":{"mode":"transition","transition":"a"},"done":{"mode":"end"}
  }}}})");
  ASSERT_TRUE(config);
  auto set = AnimatedPartSet::create(*config, 1);
  ASSERT_TRUE(set);
  auto finish = set->finish_animations();
  ASSERT_FALSE(finish);
  EXPECT_TRUE(finish.error().is<AnimatedPartSetException>());
  ASSERT_TRUE(set->set_active_state("movement", "done"));
  EXPECT_TRUE(set->finish_animations());
});

const GameTestRegistration assets_animation_duration_limits("AssetsTest", "AnimationRejectsInvalidFrameAndCycleBounds", [](GameTest&) {
  auto create = [](Json frames, Json cycle, String const& mode) {
    JsonObject state{{"frames", std::move(frames)}, {"cycle", std::move(cycle)}, {"mode", mode}};
    JsonObject type{{"default", "idle"}, {"states", JsonObject{{"idle", std::move(state)}}}};
    return AnimatedPartSet::create(JsonObject{{"stateTypes", JsonObject{{"movement", std::move(type)}}}}, 1);
  };
  EXPECT_FALSE(create(0, 1, "end"));
  EXPECT_FALSE(create(-1, 1, "end"));
  EXPECT_FALSE(create(std::int64_t{2147483648}, 1, "end"));
  EXPECT_FALSE(create(2, 0, "end"));
  EXPECT_FALSE(create(1, 0, "loop"));
  EXPECT_FALSE(create(1, -1, "end"));
  // A fast-math caller need not materialize literal infinity or NaN arguments.
  for (auto bits : {0x7ff0000000000000ULL, 0x7ff8000000000001ULL}) {
    volatile std::uint64_t input = bits;
    Json cycle(std::bit_cast<double>(static_cast<std::uint64_t>(input)));
    auto stored = cycle.to_double();
    ASSERT_TRUE(stored);
    ASSERT_EQ(std::bit_cast<std::uint64_t>(*stored), bits);
    EXPECT_FALSE(create(1, std::move(cycle), "end"));
  }
  auto set = create(1, 0, "end");
  ASSERT_TRUE(set);
  ASSERT_TRUE(set->update(1));
  AnimatedPartSet::Sample sample;
  set->prepare_sample(sample, 0.5f);
  auto frame = set->active_state("movement", sample);
  ASSERT_TRUE(frame);
  EXPECT_EQ(frame->get().frame, 0u);
  EXPECT_FLOAT_EQ(frame->get().frame_progress, 0.f);
});

const GameTestRegistration assets_animation_sample_validation("AssetsTest", "AnimationSampleErrorsAreRecoverable", [](GameTest&) {
  auto config = Json::parse(R"({"stateTypes":{"movement":{"default":"idle","states":{"idle":{}}}},"parts":{"body":{}}})");
  auto other_config = Json::parse(R"({"stateTypes":{"color":{"default":"on","states":{"on":{}}}},"parts":{"head":{}}})");
  ASSERT_TRUE(config);
  ASSERT_TRUE(other_config);
  auto set = AnimatedPartSet::create(*config, 1);
  auto other = AnimatedPartSet::create(*other_config, 1);
  ASSERT_TRUE(set);
  ASSERT_TRUE(other);
  AnimatedPartSet::Sample sample;
  EXPECT_FALSE(set->active_state("movement", sample));
  EXPECT_FALSE(set->active_part("body", sample));
  EXPECT_FALSE(set->set_presentation_state("movement", "idle", true, false, sample));
  other->prepare_sample(sample, 1);
  EXPECT_FALSE(set->active_part("body", sample));
  EXPECT_FALSE(set->active_state("movement", sample));
  set->prepare_sample(sample, 1);
  EXPECT_TRUE(set->active_part("body", sample));
  EXPECT_TRUE(set->active_state("movement", sample));
  auto copy = *set;
  EXPECT_FALSE(copy.active_state("movement", sample));
  unsigned visits = 0;
  auto visit = [&](String const&, auto const&) -> std::expected<void, StarException> {
    ++visits;
    return {};
  };
  EXPECT_FALSE(copy.for_each_active_part(visit, sample));
  EXPECT_EQ(visits, 0u);
  copy.prepare_sample(sample, 1);
  EXPECT_TRUE(copy.for_each_active_part(visit, sample));
  EXPECT_EQ(visits, 1u);
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
    animation->prepare_sample(sample, 1);
    auto currentOnly = animation->active_part("body", sample);
    ASSERT_TRUE(currentOnly);
    EXPECT_FLOAT_EQ(currentOnly->get().x_translation_animation, 10);
    animation->prepare_sample(sample, 1);
    ASSERT_TRUE(animation->active_part("body", sample));
    auto definition = animation->parts().get("body");
    ASSERT_TRUE(definition);
    definition->get().part_properties["interpolated"] = true;
    definition->get().active_part_dirty = true;
    animation->prepare_sample(sample, 1);
    auto invalidNext = animation->active_part("body", sample);
    ASSERT_FALSE(invalidNext);
    EXPECT_TRUE(invalidNext.error().is<JsonException>());
    definition->get().part_properties["interpolated"] = false;
    definition->get().active_part_dirty = true;
    auto recovered = animation->active_part("body", sample);
    ASSERT_TRUE(recovered);
    EXPECT_FLOAT_EQ(recovered->get().x_translation_animation, 10);
    ASSERT_TRUE(animation->update(1));
    animation->prepare_sample(sample, 1);
    auto invalidCurrent = animation->active_part("body", sample);
    ASSERT_FALSE(invalidCurrent);
    EXPECT_TRUE(invalidCurrent.error().is<JsonException>());
    ASSERT_TRUE(animation->set_state_type_enabled("movement", false));
    animation->prepare_sample(sample, 1);
    auto unused = animation->active_part("body", sample);
    ASSERT_TRUE(unused);
    EXPECT_FALSE(unused->get().active_state);
    EXPECT_FLOAT_EQ(unused->get().x_translation_animation, 0);
  });

const GameTestRegistration effectSourceParticleCache(
  "EffectSourceTest", "LazyParticleChoicesRetainOwnedResultsAndStageErrors", [](GameTest& fixture) {
    Json definition = JsonObject{{"duration", 1},
                                 {"durationVariance", 0},
                                 {"loops", true},
                                 {"start", JsonObject{{"particles", JsonArray{JsonArray{"start-a", "start-b"}}}}},
                                 {"particles", JsonArray{JsonArray{"loop"}}},
                                 {"stop", JsonObject{{"particles", JsonArray{7}}}}};
    auto source = EffectSource::create(fixture.root(), "normal", definition);
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
    auto replacement = EffectSource::create(fixture.root(), "normal", *repaired);
    ASSERT_TRUE(replacement);
    (*replacement)->finishEventCollection();
    (*replacement)->stop();
    (*replacement)->tick(0);
    auto freshFinal = (*replacement)->particles();
    ASSERT_TRUE(freshFinal);
    EXPECT_EQ(*freshFinal, (List<String>{"stop"}));
  });

const GameTestRegistration effect_duplicate_kinds("EffectSourceTest", "RejectsCaseAliasesInEitherFileOrder", [](GameTest& fixture) {
  for (bool reversed : {false, true}) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    String mixed = R"({"kind":"Spark"})";
    String lower = R"({"kind":"spark"})";
    auto root = effect_test_root(fixture, *directory, {
      {"first.effectsource", reversed ? mixed : lower}, {"second.effectsource", reversed ? lower : mixed}});
    ASSERT_TRUE(root) << root.error().what();
    auto database = (*root)->effectSourceDatabase();
    ASSERT_FALSE(database);
    EXPECT_NE(std::string_view(database.error().what()).find("Duplicate effect source"), std::string_view::npos);
  }
});

const GameTestRegistration particle_catalog_duplicate_kinds(
  "ParticleDatabase", "CaseAliasesAttributeDuplicatesToTheSecondDefinition", [](GameTest& fixture) {
    for (bool reversed : {false, true}) {
      SCOPED_TRACE(reversed);
      auto directory = File::temporary_directory(fixture.root().random());
      ASSERT_TRUE(directory);
      auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
      String mixed = R"({"kind":"Spark","definition":{}})";
      String lower = R"({"kind":"spark","definition":{}})";
      auto root = effect_test_root(fixture, *directory, {
          {"first.particle", reversed ? mixed : lower}, {"second.particle", reversed ? lower : mixed}});
      ASSERT_TRUE(root) << root.error().what();
      auto assets = (*root)->assets();
      ASSERT_TRUE(assets) << assets.error().what();
      auto files = (*assets)->scan_extension("particle");
      ASSERT_TRUE(files) << files.error().what();
      ASSERT_EQ(files->get().size(), 2u);
      auto const& duplicate_path = std::next(files->get().begin())->string();
      auto duplicate = (*assets)->json(duplicate_path);
      ASSERT_TRUE(duplicate) << duplicate.error().what();
      auto duplicate_kind = duplicate->get_string("kind");
      ASSERT_TRUE(duplicate_kind) << duplicate_kind.error().what();

      auto database = (*root)->particle_database();
      ASSERT_FALSE(database);
      auto const& summary = database.error().diagnostic_summary();
      ASSERT_TRUE(summary);
      EXPECT_EQ(summary->asset, duplicate_path.utf8());
      EXPECT_EQ(summary->operation, "Loading particle");
      EXPECT_TRUE(summary->asset_provider.contains(File::relative_to(*directory, "assets").utf8()));
      EXPECT_TRUE(summary->reason.contains(Star::format("Duplicate particle asset kind Name {}.", *duplicate_kind)));
    }
  });

const GameTestRegistration particle_catalog_variants(
  "ParticleDatabase", "NamedAndInlineDirectAndReusableParticlesMatchEveryDraw", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    String definition = R"({
      "type":"textured","image":"spark.png","size":20,"position":[21,22],
      "initialVelocity":[23,24],"finalVelocity":[25,26],"rotation":60,"angularVelocity":120,
      "approach":[27,28],"length":29,"timeToLive":30,
      "variance":{"size":1,"position":[2,3],"initialVelocity":[4,5],"finalVelocity":[6,7],
        "rotation":30,"angularVelocity":45,"length":10,"timeToLive":11}
    })";
    auto root = effect_test_root(fixture, *directory, {
        {"spark.particle", String(R"({"kind":"SpArK","definition":)") + definition + "}"}});
    ASSERT_TRUE(root) << root.error().what();
    auto database = (*root)->particle_database();
    ASSERT_TRUE(database) << database.error().what();
    auto mixed_config = (*database)->config("SpArK");
    ASSERT_TRUE(mixed_config) << mixed_config.error().what();
    EXPECT_EQ((*mixed_config)->kind(), String("SpArK"));
    for (String const& spelling : {String("spark"), String("SPARK"), String("sPaRk")}) {
      auto config = (*database)->config(spelling);
      ASSERT_TRUE(config) << config.error().what();
      EXPECT_EQ(*config, *mixed_config);
    }
    EXPECT_FALSE((*database)->config("missing"));

    auto inline_config = Json::parse(definition);
    ASSERT_TRUE(inline_config) << inline_config.error().what();
    auto zero_variance_config = Json::parse(R"({"type":"ember","size":20,"variance":{"length":0}})");
    ASSERT_TRUE(zero_variance_config) << zero_variance_config.error().what();
    Json const name("sPaRk");
    constexpr std::uint64_t seed = 0x535041524bull;
    auto& random = (*root)->random();
    struct VariantCase {
      Json const& specification;
      Json const& definition;
      String relative_path;
      bool zero_variance = false;
    };
    for (auto const& test : std::array{
        VariantCase{name, *inline_config, ""},
        VariantCase{*inline_config, *inline_config, "/local/definition.animation"},
        VariantCase{*zero_variance_config, *zero_variance_config, "", true}}) {
      auto base = Particle::create(**root, test.definition, test.relative_path);
      ASSERT_TRUE(base) << base.error().what();
      random.seed(seed);
      RandomSource oracle(seed);
      auto creator = (*database)->particle_creator(test.specification, test.relative_path);
      ASSERT_TRUE(creator) << creator.error().what();
      for (unsigned trial = 0; trial != 4; ++trial) {
        std::array<float, 11> draws;
        for (auto& draw : draws)
          draw = oracle.uniform_float() * 2.0f - 1.0f;
        Particle actual;
        if ((trial & 1) == 0) {
          auto direct = (*database)->particle(test.specification, test.relative_path);
          ASSERT_TRUE(direct) << direct.error().what();
          actual = std::move(*direct);
        } else {
          actual = (*creator)();
        }
        auto delta = [&](std::size_t index, float scale) { return test.zero_variance ? 0.0f : draws[index] * scale; };
        EXPECT_FLOAT_EQ(actual.size, base->size + delta(0, 1));
        EXPECT_FLOAT_EQ(actual.position[0], base->position[0] + delta(1, 2));
        EXPECT_FLOAT_EQ(actual.position[1], base->position[1] + delta(2, 3));
        EXPECT_FLOAT_EQ(actual.velocity[0], base->velocity[0] + delta(3, 4));
        EXPECT_FLOAT_EQ(actual.velocity[1], base->velocity[1] + delta(4, 5));
        EXPECT_FLOAT_EQ(actual.final_velocity[0], base->final_velocity[0] + delta(5, 6));
        EXPECT_FLOAT_EQ(actual.final_velocity[1], base->final_velocity[1] + delta(6, 7));
        EXPECT_FLOAT_EQ(actual.rotation, base->rotation + delta(7, 30 * std::numbers::pi / 180.0f));
        EXPECT_FLOAT_EQ(actual.angular_velocity, base->angular_velocity + delta(8, 45 * std::numbers::pi / 180.0f));
        EXPECT_FLOAT_EQ(actual.length, base->length + delta(9, 10));
        EXPECT_FLOAT_EQ(actual.time_to_live, base->time_to_live + delta(10, 11));
        EXPECT_EQ(actual.base_size, base->base_size);
        EXPECT_EQ(actual.approach, base->approach);
        EXPECT_EQ(actual.string, base->string);
        EXPECT_EQ(random.next_u64(), oracle.next_u64());
      }
    }
  });

const GameTestRegistration effect_identity_wire("EffectSourceTest", "CanonicalRunningIdentityPreservesRawWireAndDisk", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory);
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto root = effect_test_root(fixture, *directory, {
    {"spark.effectsource", R"({"kind":"Spark","definition":{"duration":200,"start":{"sounds":"/loop.wav"}}})"},
    {"pulse.effectsource", R"({"kind":"Pulse","definition":{"start":{"sounds":"/loop.wav"}}})"},
    {"invalid.particle", R"({"kind":false})"}});
  ASSERT_TRUE(root) << root.error().what();
  auto master = EffectEmitter::create(**root);
  auto slave = EffectEmitter::create(**root);
  ASSERT_TRUE(master);
  ASSERT_TRUE(slave);
  EffectEventSink sink;
  ASSERT_TRUE((*master)->collect_render_events(sink));
  ASSERT_TRUE((*slave)->collect_render_events(sink));
  (*master)->add_effect_sources("hand", {"SPARK", "spark"});
  ASSERT_TRUE((*master)->tick(0, EntityMode::Master));
  auto disk = Json::parse(R"({"activeSources":[{"position":"hand","source":"SPARK"},{"position":"hand","source":"spark"}]})");
  ASSERT_TRUE(disk);
  EXPECT_EQ((*master)->to_json(), *disk);
  Set<std::pair<String, String>> const raw{{"hand", "SPARK"}, {"hand", "spark"}};
  DataStreamBuffer packet, legacy;
  ASSERT_TRUE(legacy << raw);
  ASSERT_TRUE((*master)->net_store(packet, {}));
  EXPECT_TRUE(std::ranges::equal(packet.span(), legacy.span()));
  ASSERT_TRUE(packet.seek(0));
  ASSERT_TRUE((*slave)->net_load(packet, {}));
  ASSERT_TRUE((*slave)->tick(0, EntityMode::Slave));
  EXPECT_FALSE((*slave)->collect_render_events(sink)); // Position errors still precede event work.
  (*slave)->set_source_position("hand", {1, 2});
  ASSERT_TRUE((*slave)->collect_render_events(sink)); // Pure audio must not load the invalid particle DB.
  ASSERT_EQ(sink.accepted, "a");
  for (auto const& spelling : StringList{"sParK", "spark", "SPARK"}) {
    Json updated = JsonObject{{"activeSources", JsonArray{JsonObject{{"position", "hand"}, {"source", spelling}}}}};
    ASSERT_TRUE((*slave)->from_json(updated));
    ASSERT_TRUE((*slave)->tick(0, EntityMode::Slave));
    ASSERT_TRUE((*slave)->collect_render_events(sink));
    EXPECT_EQ((*slave)->to_json(), updated);
    EXPECT_EQ(sink.accepted, "a");
  }
  // Expiry, unlike a mere case change, must restart a still-requested one-shot.
  (*master)->reset();
  (*master)->set_source_position("hand", {5, 6});
  for (std::size_t frame = 0; frame != 5; ++frame) {
    (*master)->add_effect_sources("hand", {"PULSE"});
    ASSERT_TRUE((*master)->tick(0, EntityMode::Master));
    ASSERT_TRUE((*master)->collect_render_events(sink));
  }
  EXPECT_EQ(sink.accepted, "aaaa"); // start, final, start, final, start

  // A failed reconciliation is retried even with an unchanged network field.
  (*master)->reset();
  for (std::size_t frame = 0; frame != 2; ++frame) {
    (*master)->add_effect_sources("hand", {"Spark", "unknown"});
    EXPECT_FALSE((*master)->tick(0, EntityMode::Master));
    ASSERT_TRUE((*master)->collect_render_events(sink));
    EXPECT_EQ(sink.accepted, "aaaaa");
  }
});

const GameTestRegistration effect_emission_retry("EffectSourceTest", "RetriesOnlyUnacceptedEventsWithoutNewDraws", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory);
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto root = effect_test_root(fixture, *directory, {
    {"sample.particle", R"({"kind":"sample","definition":{"type":"ember","variance":{"position":[1,1]}}})"},
    {"burst.effectsource", R"({
      "kind":"Burst","definition":{"duration":100,"start":{"particles":[["sample","sample"]]},
      "sounds":[[{"resource":"/loop.wav","loops":-1,"pitchVariability":0.2},
                 {"resource":"/loop.wav","loops":-1,"pitchVariability":0.2}]]}})"}});
  ASSERT_TRUE(root) << root.error().what();
  // Database construction is outside the compared RNG sequences.
  ASSERT_TRUE((*root)->effectSourceDatabase());
  ASSERT_TRUE((*root)->particle_database());
  List<Vec2F> baseline_particles;
  std::uint64_t baseline_rng = 0;
  for (auto failure : std::array<std::optional<std::size_t>, 4>{std::nullopt, 1u, 3u, 4u}) {
    (*root)->random().seed(0x535041524bull);
    auto emitter = EffectEmitter::create(**root);
    ASSERT_TRUE(emitter);
    EffectEventSink sink;
    ASSERT_TRUE((*emitter)->collect_render_events(sink));
    (*emitter)->set_source_position("a", {5, 6});
    (*emitter)->set_source_position("b", {7, 8});
    (*emitter)->add_effect_sources("a", {"burst"});
    (*emitter)->add_effect_sources("b", {"BURST"});
    ASSERT_TRUE((*emitter)->tick(0, EntityMode::Master));
    sink.fail_at = failure;
    auto result = (*emitter)->collect_render_events(sink);
    if (failure) {
      ASSERT_FALSE(result);
      ASSERT_EQ(sink.accepted.size(), *failure);
      auto particle_index = sink.particles.size();
      auto audio_index = sink.sounds.size();
      ASSERT_TRUE((*emitter)->collect_render_events(sink));
      if (sink.rejected_particle) {
        ASSERT_LT(particle_index, sink.particles.size());
        EXPECT_EQ(sink.particles[particle_index], *sink.rejected_particle);
      }
      if (sink.rejected_audio) {
        ASSERT_LT(audio_index, sink.sounds.size());
        EXPECT_EQ(sink.sounds[audio_index], sink.rejected_audio);
      }
    } else {
      ASSERT_TRUE(result) << result.error().what();
    }
    ASSERT_EQ(sink.accepted, "ppaappaa");
    ASSERT_TRUE((*emitter)->collect_render_events(sink));
    ASSERT_EQ(sink.accepted, "ppaappaa");
    auto next = (*root)->random().next_u64();
    if (!failure) {
      baseline_particles = sink.particles;
      baseline_rng = next;
    } else {
      EXPECT_EQ(sink.particles, baseline_particles);
      EXPECT_EQ(next, baseline_rng);
    }
    // Stopping the sources must still stop the exact main loops handed to the sink.
    Mixer mixer((*root)->logger(), 8000, 1);
    for (auto const& sound : sink.sounds) {
      EXPECT_EQ(sound->loops(), -1);
      ASSERT_TRUE(mixer.play(sound));
    }
    for (std::size_t frame = 0; frame < 2; ++frame) {
      ASSERT_TRUE((*emitter)->tick(0, EntityMode::Master));
      ASSERT_TRUE((*emitter)->collect_render_events(sink));
    }
    std::array<std::int16_t, 8> samples;
    ASSERT_TRUE(mixer.read(samples, samples.size()));
    for (auto const& sound : sink.sounds)
      EXPECT_TRUE(sound->finished());
  }
  // A reset discards a rejected submission, even when the same source is requested again.
  auto emitter = EffectEmitter::create(**root);
  ASSERT_TRUE(emitter);
  EffectEventSink sink;
  ASSERT_TRUE((*emitter)->collect_render_events(sink));
  (*emitter)->set_source_position("a", {0, 0});
  (*emitter)->add_effect_sources("a", {"burst"});
  ASSERT_TRUE((*emitter)->tick(0, EntityMode::Master));
  sink.fail_at = 1;
  ASSERT_FALSE((*emitter)->collect_render_events(sink));
  (*emitter)->reset();
  (*emitter)->set_source_position("a", {100, 100});
  (*emitter)->add_effect_sources("a", {"burst"});
  ASSERT_TRUE((*emitter)->tick(0, EntityMode::Master));
  ASSERT_TRUE((*emitter)->collect_render_events(sink));
  EXPECT_EQ(sink.accepted, "pppaa");
  ASSERT_EQ(sink.particles.size(), 3u);
  EXPECT_GT(sink.particles[1][0], 98);
});

const GameTestRegistration object_event_retry("ObjectRenderTest", "RetriesRejectedItemsAndPreservesRandomStream", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory);
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto root = effect_test_root(fixture, *directory, {
    {"event-retry-probe.object", R"({
      "objectName":"event-retry-probe","hasObjectItem":false,"health":10,
      "soundEffect":"/loop.wav","soundEffectRangeMultiplier":4,
      "damageTable":{"damageFactors":{"blockish":1},"damageRecovery":0,"maximumEffectTime":1},
      "orientations":[{"imageLayers":[],"spaces":[[0,0],[1,0]],"anchors":[]}],
      "particleEmitters":[
        {"emissionRate":1,"emissionVariance":0.2,"placeInSpaces":true,
         "particle":{"type":"text","text":"configured-a"},"particleVariance":{"position":[1,1]}},
        {"emissionRate":1,"emissionVariance":0.2,
         "particle":{"type":"text","text":"configured-b"},"particleVariance":{"position":[1,1]}}
      ],
      "animation":{"animatedParts":{"stateTypes":{"probe":{"default":"idle","states":{
        "idle":{"properties":{"persistentSound":"/loop.wav","persistentSoundRangeMultiplier":2,
                              "immediateSound":"/loop.wav","immediateSoundRangeMultiplier":3}}
      }}}},"particleEmitters":{"probe":{"active":true,"particles":[
        {"particle":{"type":"text","text":"animator-a","variance":{"position":[1,1]}}},
        {"particle":{"type":"text","text":"animator-b","variance":{"position":[1,1]}}}
      ]}}},
      "animationScripts":["/event-retry-probe.lua"]
    })"},
    {"event-retry-probe.lua", R"(
      function init()
        localAnimator.spawnParticle({type="text", text="scripted-a", variance={position={1,1}}}, {10,20})
        localAnimator.spawnParticle({type="text", text="scripted-b", variance={position={1,1}}}, {30,40})
        localAnimator.playAudio("/loop.wav", 0, 0.25)
        localAnimator.playAudio("/loop.wav", 0, 0.75)
      end
    )"}}, true);
  ASSERT_TRUE(root) << root.error().what();
  auto& game = **root;
  auto assets = game.assets();
  ASSERT_TRUE(assets) << assets.error().what();
  ASSERT_TRUE((*assets)->audio("/loop.wav"));
  ASSERT_TRUE(game.particle_database());

  // Start a real headless client world by pumping its handshake without running a
  // background simulation that could consume the fixture's seeded random stream.
  auto universe = UniverseClient::create(game, {}, {}, File::relative_to(*directory, "client"));
  ASSERT_TRUE(universe) << universe.error().what();
  auto storage = File::ephemeral_file(game.random());
  ASSERT_TRUE(storage);
  auto server = WorldServer::create(game, {128, 128}, *storage);
  ASSERT_TRUE(server) << server.error().what();
  auto close_server = make_scope_exit([&] { EXPECT_TRUE((*server)->close()); });
  auto client = WorldClient::create(game, main_client_world_id, **universe);
  ASSERT_TRUE(client) << client.error().what();
  auto close_client = make_scope_exit([&] { EXPECT_TRUE((*client)->close()); });
  auto accepted = (*server)->addClient(min_client_connection_id, SpawnTargetPosition(Vec2F(31, 42)), true);
  ASSERT_TRUE(accepted) << accepted.error().what();
  ASSERT_TRUE(*accepted);
  auto start_packets = (*server)->getOutgoingPackets(min_client_connection_id);
  ASSERT_TRUE(start_packets) << start_packets.error().what();
  auto started = (*client)->handleIncomingPackets(*start_packets);
  ASSERT_TRUE(started) << started.error().what();
  auto acknowledged = (*server)->handleIncomingPackets(min_client_connection_id, (*client)->getOutgoingPackets());
  ASSERT_TRUE(acknowledged) << acknowledged.error().what();
  ASSERT_EQ((*client)->connection(), min_client_connection_id);
  auto objects = game.object_database();
  ASSERT_TRUE(objects) << objects.error().what();
  auto master = (*objects)->create_object("event-retry-probe");
  ASSERT_TRUE(master) << master.error().what();
  TileEntity& master_tile = **master;
  ASSERT_TRUE(master_tile.set_tile_position({31, 42}));
  ASSERT_TRUE((*master)->init(**server, 1, EntityMode::Master));
  auto release_master = make_scope_exit([&] { EXPECT_TRUE((*master)->uninit()); });
  auto stored = (*master)->net_store();
  ASSERT_TRUE(stored) << stored.error().what();
  auto state = (*master)->write_net_state();
  ASSERT_TRUE(state) << state.error().what();

  using SoundFingerprint = std::tuple<int, float, std::optional<Vec2F>, double>;
  List<Vec2F> baseline_particles;
  List<SoundFingerprint> baseline_sounds;
  std::uint64_t baseline_rng = 0;
  for (auto failure : std::array<std::optional<std::size_t>, 7>{std::nullopt, 1u, 2u, 4u, 6u, 8u, 10u}) {
    SCOPED_TRACE(failure.value_or(11u));
    auto object = (*objects)->net_load_object(*stored);
    ASSERT_TRUE(object) << object.error().what();
    ASSERT_TRUE((*object)->read_net_state(state->first));
    game.random().seed(0x4f424a454354ull);
    ASSERT_TRUE((*object)->init(**client, 1, EntityMode::Slave));
    auto release_object = make_scope_exit([&] {
      if ((*object)->inWorld())
        EXPECT_TRUE((*object)->uninit());
    });
    TileEntity& tile = **object;
    auto damaged = tile.damage_tiles({}, {}, {TileDamageType::Blockish, 1.0f});
    ASSERT_TRUE(damaged) << damaged.error().what();
    ASSERT_FALSE(*damaged);
    auto updated = (*object)->update(0, 0);
    ASSERT_TRUE(updated) << updated.error().what();

    struct ObjectEventSink : EffectEventSink {
      StringList labels;
      std::expected<void, StarException> add_particle(Particle&& particle) override {
        auto submitted = EffectEventSink::add_particle(std::move(particle));
        if (submitted)
          labels.push_back(std::move(particle.string));
        return submitted;
      }
    } sink;
    if (failure) {
      sink.fail_at = failure;
      ASSERT_FALSE((*object)->collect_render_events(sink));
      ASSERT_EQ(sink.accepted.size(), *failure);
      auto const particle_index = sink.particles.size();
      auto const audio_index = sink.sounds.size();
      auto const rejected_particle = sink.rejected_particle;
      auto const rejected_audio = sink.rejected_audio;
      sink.fail_at = failure;
      ASSERT_FALSE((*object)->collect_render_events(sink));
      ASSERT_EQ(sink.accepted.size(), *failure);
      EXPECT_EQ(sink.rejected_particle, rejected_particle);
      EXPECT_EQ(sink.rejected_audio, rejected_audio);
      ASSERT_TRUE((*object)->collect_render_events(sink));
      if (rejected_particle) {
        ASSERT_LT(particle_index, sink.particles.size());
        EXPECT_EQ(sink.particles[particle_index], *rejected_particle);
      } else {
        ASSERT_TRUE(rejected_audio);
        ASSERT_LT(audio_index, sink.sounds.size());
        EXPECT_EQ(sink.sounds[audio_index], rejected_audio);
      }
    } else {
      auto result = (*object)->collect_render_events(sink);
      ASSERT_TRUE(result) << result.error().what();
    }
    ASSERT_EQ(sink.accepted, "ppaaappppaa");
    EXPECT_EQ(sink.labels, (StringList{"configured-a", "configured-b", "animator-a", "animator-b", "scripted-a", "scripted-b"}));
    ASSERT_TRUE((*object)->collect_render_events(sink));
    ASSERT_EQ(sink.accepted, "ppaaappppaa");
    auto next_rng = game.random().next_u64();
    List<SoundFingerprint> sounds;
    for (auto const& sound : sink.sounds) {
      auto time = sound->current_time();
      ASSERT_TRUE(time) << time.error().what();
      sounds.emplace_back(sound->loops(), sound->range_multiplier(), sound->position(), *time);
    }
    EXPECT_EQ(sink.sounds[0]->range_multiplier(), 4.0f);
    EXPECT_EQ(sink.sounds[1]->range_multiplier(), 2.0f);
    EXPECT_EQ(sink.sounds[2]->range_multiplier(), 3.0f);
    if (!failure) {
      baseline_particles = sink.particles;
      baseline_sounds = std::move(sounds);
      baseline_rng = next_rng;
    } else {
      EXPECT_EQ(sink.particles, baseline_particles);
      EXPECT_EQ(sounds, baseline_sounds);
      EXPECT_EQ(next_rng, baseline_rng);
    }
    Mixer mixer(game.logger(), 8000, 1);
    ASSERT_EQ(sink.sounds[0]->loops(), -1);
    ASSERT_TRUE(mixer.play(sink.sounds[0]));
    ASSERT_TRUE((*object)->uninit());
    std::array<std::int16_t, 8> output{};
    ASSERT_TRUE(mixer.read(output, output.size()));
    EXPECT_TRUE(sink.sounds[0]->finished());
  }
});

const GameTestRegistration effect_audio_repair("EffectSourceTest", "AudioRepairResumesBatchAndPreservesLaterTick", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory);
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto root = effect_test_root(fixture, *directory, {
    {"sample.particle", R"({"kind":"sample","definition":{"type":"ember"}})"},
    {"broken.wav", "invalid audio"},
    {"burst.effectsource", R"({
      "kind":"Burst","definition":{"duration":1,"particles":[["sample"]],
      "start":{"particles":[["sample"]]},"sounds":[["/loop.wav","/broken.wav"]]}})"}});
  ASSERT_TRUE(root) << root.error().what();
  auto emitter = EffectEmitter::create(**root);
  ASSERT_TRUE(emitter);
  EffectEventSink sink;
  ASSERT_TRUE((*emitter)->collect_render_events(sink));
  (*emitter)->set_source_position("hand", {2, 3});
  (*emitter)->add_effect_sources("hand", {"Burst"});
  ASSERT_TRUE((*emitter)->tick(0, EntityMode::Master));
  ASSERT_FALSE((*emitter)->collect_render_events(sink));
  ASSERT_EQ(sink.accepted, "p");
  (*emitter)->set_source_position("hand", {12, 13});
  (*emitter)->add_effect_sources("hand", {"BURST"});
  ASSERT_TRUE((*emitter)->tick(2, EntityMode::Master));
  auto assets = (*root)->assets();
  ASSERT_TRUE(assets);
  auto wave = std::bit_cast<std::array<char, pcm_test_wave.size()>>(pcm_test_wave);
  ASSERT_TRUE(File::write_file(std::span<char const>(wave), File::relative_to(*directory, "assets/broken.wav")));
  (*assets)->hot_reload();
  auto resumed = (*emitter)->collect_render_events(sink);
  ASSERT_TRUE(resumed) << resumed.error().what();
  EXPECT_EQ(sink.accepted, "paap");
  EXPECT_EQ(sink.particles, (List<Vec2F>{{2, 3}, {12, 13}}));
  ASSERT_EQ(sink.sounds.size(), 2u);
  for (auto const& sound : sink.sounds)
    EXPECT_EQ(sound->position(), Vec2F(2, 3));
});

const GameTestRegistration assetsWavSignaturesAndPcmSamples("AssetsTest", "WavSignaturesAndPcmSamples", [](GameTest&) {
  auto bytes = pcm_test_wave;
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
  EXPECT_EQ(audio->sample_rate(), 8000u);
  std::array<std::int16_t, 2> samples{};
  auto read = audio->read(samples);
  ASSERT_TRUE(read);
  ASSERT_EQ(*read, samples.size());
  EXPECT_EQ(samples[0], 0x1234);
  EXPECT_EQ(samples[1], -2);

  ASSERT_TRUE(audio->seek_sample(1));
  auto cloned = audio->clone();
  ASSERT_TRUE(cloned);
  ASSERT_TRUE(audio->seek_sample(0));
  EXPECT_EQ(cloned->current_sample(), 1u);
  Audio moved(std::move(*audio));
  *audio = std::move(*cloned);
  // Replacing the original decoder must not invalidate a clone's shared PCM storage.
  ASSERT_TRUE(audio->seek_sample(0));
  moved = std::move(*audio);
  EXPECT_FALSE(moved.seek_sample(3));
  EXPECT_EQ(moved.current_sample(), 0u);
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
  auto input = (*assets)->open_file("/sfx/interface/voice_on.ogg");
  ASSERT_TRUE(input);
  auto audio = Audio::create(*input, "mixer-group.ogg");
  ASSERT_TRUE(audio);
  auto instance = AudioInstance::create(*audio);
  ASSERT_TRUE(instance);
  auto engine = LuaEngine::create();
  ASSERT_TRUE(engine);
  auto context = (*engine)->create_context();
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
    auto selected = context->invoke_path<String>("selectGroup", String(inputName));
    ASSERT_TRUE(selected);
    EXPECT_EQ((*instance)->mixer_group(), group);
    EXPECT_EQ(selected->utf8(), canonical);
  }
  for (auto const& invalid : {String("unknown"), String("Músic"), String(std::string("Music\0Effects", 13)),
                              String(std::string(1, static_cast<char>(0xff)))}) {
    auto selected = context->invoke_path<String>("selectGroup", invalid);
    EXPECT_FALSE(selected);
    EXPECT_EQ((*instance)->mixer_group(), MixerGroup::Instruments);
  }
  auto recovered = context->invoke_path<String>("selectGroup", String("effects"));
  ASSERT_TRUE(recovered);
  EXPECT_EQ(*recovered, "Effects");
  EXPECT_EQ((*instance)->mixer_group(), MixerGroup::Effects);
});

const GameTestRegistration assetsOggCloneLifetimes("AssetsTest", "OggCloneIndependentCursorsAndLifetime", [](GameTest& fixture) {
  auto assets = fixture.root().assets();
  ASSERT_TRUE(assets);
  auto input = (*assets)->open_file("/sfx/interface/voice_on.ogg");
  ASSERT_TRUE(input);
  auto created = Audio::create(std::move(*input), "clone-lifetime.ogg");
  ASSERT_TRUE(created);
  std::optional<Audio> owner(std::move(*created));
  ASSERT_TRUE(owner->compressed());
  auto total = owner->total_samples();
  ASSERT_TRUE(total);
  ASSERT_GT(*total, 0u);
  ASSERT_TRUE(owner->seek_sample(*total / 2));
  auto cloned = owner->clone();
  ASSERT_TRUE(cloned);
  EXPECT_EQ(cloned->current_sample(), *total / 2);
  ASSERT_TRUE(owner->seek_sample(0));
  EXPECT_EQ(cloned->current_sample(), *total / 2);

  // A rejected seek leaves the original decoder usable at its existing cursor.
  EXPECT_FALSE(owner->seek_sample(*total + 1));
  EXPECT_EQ(owner->current_sample(), 0u);
  std::array<std::int16_t, 64> original{};
  auto read = owner->read(original);
  ASSERT_TRUE(read);
  ASSERT_GT(*read, 0u);
  owner.reset();
  ASSERT_TRUE(cloned->seek_sample(0));
  std::array<std::int16_t, 64> replay{};
  auto replayed = cloned->read(replay);
  ASSERT_TRUE(replayed);
  EXPECT_EQ(*replayed, *read);
  EXPECT_EQ(replay, original);

  auto pivot = *total / 2;
  ASSERT_TRUE(cloned->seek_sample(pivot));
  auto compressed_reference = cloned->clone();
  ASSERT_TRUE(compressed_reference);
  std::array<std::int16_t, 64> suffix{};
  auto suffix_count = compressed_reference->read(suffix);
  ASSERT_TRUE(suffix_count);
  ASSERT_TRUE(cloned->uncompress());
  EXPECT_EQ(cloned->total_samples(), *total);
  EXPECT_EQ(cloned->current_sample(), pivot);
  replay.fill(0);
  replayed = cloned->read(replay);
  ASSERT_TRUE(replayed);
  EXPECT_EQ(*replayed, *suffix_count);
  EXPECT_EQ(replay, suffix);

  ASSERT_TRUE(compressed_reference->seek_sample(*total));
  ASSERT_TRUE(compressed_reference->uncompress());
  EXPECT_EQ(compressed_reference->total_samples(), *total);
  EXPECT_EQ(compressed_reference->current_sample(), *total);
  EXPECT_EQ(compressed_reference->read(replay), 0u);

  ASSERT_TRUE(cloned->seek_sample(0));
  ASSERT_TRUE(cloned->uncompress());
  EXPECT_FALSE(cloned->compressed());
  ASSERT_TRUE(cloned->seek_sample(0));
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
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(tempDir)); });
    Assets::Settings settings{};
    settings.worker_pool_size = 4;
    settings.asset_time_to_live = 10;
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
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(tempDir)); });
    ASSERT_TRUE(File::write_file(String("[]"), File::relative_to(tempDir, "preload.config")));
    auto bytes = pngTestBytes(fixture.root().logger());
    ASSERT_TRUE(bytes);
    ASSERT_TRUE(File::write_file(*bytes, File::relative_to(tempDir, "sample.png")));
    Assets::Settings settings{};
    settings.worker_pool_size = 4;
    settings.asset_time_to_live = 10;
    for (unsigned attempt = 0; attempt < 16; ++attempt) {
      auto assets =
        Assets::create(fixture.root().logger(), fixture.root().random(), fixture.root().logMap(), settings, {tempDir});
      ASSERT_TRUE(assets);
      ASSERT_TRUE((*assets)->queue_images(StringList{"/sample.png?flipx", "/sample.png?flipy", "/sample.png"}));
    }
  });

const GameTestRegistration
  assetsQueuedLuaPatches("AssetsTest", "QueuedLuaPatchErrorsDoNotDiscardOtherResults", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
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
    settings.worker_pool_size = 4;
    settings.asset_time_to_live = 600;
    Logger logger;
    auto assets = Assets::create(logger, fixture.root().random(), fixture.root().logMap(), settings, {*directory});
    ASSERT_TRUE(assets);
    ASSERT_TRUE((*assets)->queue_jsons(paths));
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
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
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
      settings.worker_pool_size = workerCount;
      settings.asset_time_to_live = 600;
      Logger logger;
      auto assets = Assets::create(logger, fixture.root().random(), fixture.root().logMap(), settings, {*directory});
      ASSERT_TRUE(assets);
      std::set<std::int64_t> serials;
      for (unsigned begin = 0; begin < 32; begin += 8) {
        StringList batch;
        for (unsigned i = begin; i < begin + 8; ++i)
          batch.push_back(paths[i]);
        ASSERT_TRUE((*assets)->queue_jsons(batch));
        auto drained = AssetWorkerTestAccess::drain(**assets);
        ASSERT_TRUE(drained) << drained.error().what();
        for (unsigned i = begin; i < begin + 8; ++i) {
          auto loaded = (*assets)->json(paths[i]);
          if (i == 17) {
            EXPECT_FALSE(loaded);
            continue;
          }
          ASSERT_TRUE(loaded) << loaded.error().what();
          auto serial = loaded->get_int("serial");
          ASSERT_TRUE(serial);
          EXPECT_GE(*serial, 1);
          EXPECT_LE(*serial, 32);
          EXPECT_TRUE(serials.insert(*serial).second);
        }
      }
      EXPECT_EQ(serials.size(), 31u);
      auto final = (*assets)->json(paths[32]);
      ASSERT_TRUE(final) << final.error().what();
      auto count = final->get_int("serial");
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
    ASSERT_TRUE(image->write_png(fixture.root().logger(), buffer));
    ASSERT_TRUE(buffer->seek(0));
    auto metadata = Image::read_png_metadata(fixture.root().logger(), buffer);
    ASSERT_TRUE(metadata);
    auto [size, decodedFormat] = *metadata;
    EXPECT_EQ(size, image->size());
    EXPECT_EQ(decodedFormat, format);
    ASSERT_TRUE(buffer->seek(0));
    auto decoded = Image::read_png(fixture.root().logger(), buffer);
    ASSERT_TRUE(decoded);
    EXPECT_EQ(decoded->pixel_format(), format);
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
  auto headerMetadata = Image::read_png_metadata(fixture.root().logger(), std::make_shared<Buffer>(badHeader));
  ASSERT_FALSE(headerMetadata);
  EXPECT_TRUE(headerMetadata.error().is<ImageException>());
  auto headerImage = Image::read_png(fixture.root().logger(), std::make_shared<Buffer>(badHeader));
  ASSERT_FALSE(headerImage);
  EXPECT_TRUE(headerImage.error().is<ImageException>());
  auto pixelMetadata = Image::read_png_metadata(fixture.root().logger(), std::make_shared<Buffer>(badPixels));
  ASSERT_TRUE(pixelMetadata);
  auto pixelImage = Image::read_png(fixture.root().logger(), std::make_shared<Buffer>(badPixels));
  ASSERT_FALSE(pixelImage);
  EXPECT_TRUE(pixelImage.error().is<ImageException>());
});

const GameTestRegistration pngCallbacksRethrowOriginalIoException("ImageTest", "PngCallbacksRethrowOriginalIoException", [](GameTest& fixture) {
  auto bytes = pngTestBytes(fixture.root().logger());
  ASSERT_TRUE(bytes);
  auto metadata = std::make_shared<FailingPngBuffer>(*bytes, 8);
  auto metadataResult = Image::read_png_metadata(fixture.root().logger(), metadata);
  ASSERT_FALSE(metadataResult);
  EXPECT_TRUE(metadataResult.error().is<PngIoFailure>());
  EXPECT_EQ(metadata->unwound, 1u);
  auto header = std::make_shared<FailingPngBuffer>(*bytes, 8);
  auto headerResult = Image::read_png(fixture.root().logger(), header);
  ASSERT_FALSE(headerResult);
  EXPECT_TRUE(headerResult.error().is<PngIoFailure>());
  EXPECT_EQ(header->unwound, 1u);
  auto pixels = std::make_shared<FailingPngBuffer>(*bytes, 41);
  auto pixelResult = Image::read_png(fixture.root().logger(), pixels);
  ASSERT_FALSE(pixelResult);
  EXPECT_TRUE(pixelResult.error().is<PngIoFailure>());
  EXPECT_EQ(pixels->unwound, 1u);
  for (StreamOffset failAt : {0, 41}) {
    auto output = std::make_shared<FailingPngBuffer>(ByteArray{}, failAt);
    auto image = pngTestImage(PixelFormat::RGBA32);
    ASSERT_TRUE(image);
    auto written = image->write_png(fixture.root().logger(), output);
    ASSERT_FALSE(written);
    EXPECT_TRUE(written.error().is<PngIoFailure>());
    EXPECT_EQ(output->unwound, 1u);
  }
});

const GameTestRegistration pngErrorCallbackContainsDiagnosticExceptions("ImageTest", "PngErrorCallbackContainsDiagnosticExceptions", [](GameTest& fixture) {
  auto damaged = pngTestBytes(fixture.root().logger());
  ASSERT_TRUE(damaged);
  (*damaged)[29] ^= 1;
  auto metadata = Image::read_png_metadata(fixture.root().logger(), std::make_shared<FailingPngNameBuffer>(*damaged));
  ASSERT_FALSE(metadata);
  EXPECT_TRUE(metadata.error().is<PngIoFailure>());
  auto image = Image::read_png(fixture.root().logger(), std::make_shared<FailingPngNameBuffer>(*damaged));
  ASSERT_FALSE(image);
  EXPECT_TRUE(image.error().is<PngIoFailure>());
});

const GameTestRegistration invalidPngWriteThrows("ImageTest", "InvalidPngWriteThrows", [](GameTest& fixture) {
  auto output = std::make_shared<Buffer>();
  auto written = Image().write_png(fixture.root().logger(), output);
  ASSERT_FALSE(written);
  EXPECT_TRUE(written.error().is<ImageException>());
});

namespace {
struct PatchGate : LogSink {
  std::binary_semaphore entered{0};
  std::binary_semaphore resume{0};
  std::expected<void, StarException> log(std::string_view message, LogLevel) override {
    if (message == "epoch checkpoint") {
      entered.release();
      if (!resume.try_acquire_for(std::chrono::seconds(30)))
        return std::unexpected(StarException("Reload test did not release the old Lua patch"));
    }
    return {};
  }
};
}

const GameTestRegistration negative_frame_file_cache(
  "AssetsTest", "OnlyCatalogFrameMissesAreInterned", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    ASSERT_TRUE(File::write_file(String("[]"), File::relative_to(*directory, "preload.config")));
    ASSERT_TRUE(File::write_file(String(""), File::relative_to(*directory, "catalog.png")));
    Assets::Settings settings{};
    auto assets = Assets::create(fixture.root().logger(), fixture.root().random(), fixture.root().logMap(), settings, {*directory});
    ASSERT_TRUE(assets) << assets.error().what();
    for (String const& path : {String("/catalog.png"), String("/missing.png"), String("/CATALOG.png")}) {
      auto frames = (*assets)->image_frames(path);
      ASSERT_TRUE(frames) << frames.error().what();
      EXPECT_FALSE(*frames);
    }
    EXPECT_TRUE(AssetWorkerTestAccess::cached_frame_miss(**assets, "/catalog.png"));
    EXPECT_FALSE(AssetWorkerTestAccess::cached_frame_miss(**assets, "/missing.png"));
    EXPECT_FALSE(AssetWorkerTestAccess::cached_frame_miss(**assets, "/CATALOG.png"));
    (*assets)->hot_reload(); // The catalog still names exactly the same files.
    EXPECT_TRUE(AssetWorkerTestAccess::cached_frame_miss(**assets, "/catalog.png"));
  });

const GameTestRegistration reload_during_frame_patch(
  "AssetsTest", "ReloadFencesFramePublicationAndRetriesTopLevelLookup", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto path = File::relative_to(*directory, "default.frames");
    auto patch = File::relative_to(*directory, "default.frames.patch.lua");
    ASSERT_TRUE(File::write_file(String("[]"), File::relative_to(*directory, "preload.config")));
    ASSERT_TRUE(File::write_file(String(R"({"frameList":{"old":[0,0,1,1]}})"), path));
    ASSERT_TRUE(File::write_file(String(R"(function patch(data) sb.logInfo("epoch checkpoint") return data end)"), patch));
    Logger logger;
    logger.remove_stdout_sink();
    auto gate = std::make_shared<PatchGate>();
    logger.add_sink(gate);
    Assets::Settings settings{};
    auto assets = Assets::create(logger, fixture.root().random(), fixture.root().logMap(), settings, {*directory});
    ASSERT_TRUE(assets) << assets.error().what();
    auto previous = std::async(std::launch::async, [&] { return (*assets)->image_frames("/a.png"); });
    bool released = false;
    auto release = make_scope_exit([&] { if (!released) gate->resume.release(); });
    ASSERT_TRUE(gate->entered.try_acquire_for(std::chrono::seconds(30)));
    logger.remove_sink(gate);
    ASSERT_TRUE(File::remove(patch));
    ASSERT_TRUE(File::write_file(String(R"({"frameList":{"new":[0,0,1,1]}})"), path));
    (*assets)->hot_reload();
    auto current = (*assets)->image_frames("/a.png");
    ASSERT_TRUE(current && *current);
    ASSERT_TRUE((*current)->frames.contains("new"));
    gate->resume.release();
    released = true;
    auto retried = previous.get();
    ASSERT_TRUE(retried && *retried);
    EXPECT_EQ(*retried, *current);
    auto cached = (*assets)->image_frames("/a.png");
    ASSERT_TRUE(cached && *cached);
    EXPECT_EQ(*cached, *current);
  });

const GameTestRegistration reload_during_lua_patch(
  "AssetsTest", "ReloadFencesOldSuccessFailureAndNestedLuaReads", [](GameTest& fixture) {
    for (bool worker : {false, true}) {
      for (bool fail : {false, true}) {
        SCOPED_TRACE(::testing::Message() << "worker=" << worker << ", old failure=" << fail);
        auto directory = File::temporary_directory(fixture.root().random());
        ASSERT_TRUE(directory);
        auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
        auto target = File::relative_to(*directory, "target.json");
        auto dependency = File::relative_to(*directory, "dependency.json");
        auto patch = File::relative_to(*directory, "target.json.patch.lua");
        ASSERT_TRUE(File::write_file(String("[]"), File::relative_to(*directory, "preload.config")));
        auto old_json = fail ? String(R"({"epoch":1,"fail":true})") : String(R"({"epoch":1,"fail":false})");
        ASSERT_TRUE(File::write_file(old_json, target));
        ASSERT_TRUE(File::write_file(String(R"({"value":1})"), dependency));
        ASSERT_TRUE(File::write_file(String("{}"), File::relative_to(*directory, "fence.json")));
        ASSERT_TRUE(File::write_file(String(R"(
          function patch(data)
            sb.logInfo("epoch checkpoint")
            pcall(assets.json, "/dependency.json")
            if data.fail then error("failure in old snapshot") end
            return data
          end
        )"), patch));
        Logger logger;
        logger.remove_stdout_sink();
        auto gate = std::make_shared<PatchGate>();
        logger.add_sink(gate);
        Assets::Settings settings{};
        settings.worker_pool_size = worker ? 1 : 0;
        auto assets = Assets::create(logger, fixture.root().random(), fixture.root().logMap(), settings, {*directory});
        ASSERT_TRUE(assets) << assets.error().what();
        std::future<std::expected<Json, StarException>> previous;
        if (worker) {
          ASSERT_TRUE((*assets)->queue_jsons(StringList{"/target.json"}));
        } else {
          previous = std::async(std::launch::async, [&] { return (*assets)->json("/target.json"); });
        }
        bool released = false;
        auto release = make_scope_exit([&] { if (!released) gate->resume.release(); });
        ASSERT_TRUE(gate->entered.try_acquire_for(std::chrono::seconds(30)));
        // No Lua or logging in the new read: the old callback still owns those locks.
        logger.remove_sink(gate);
        ASSERT_TRUE(File::remove(patch));
        ASSERT_TRUE(File::write_file(String(R"({"epoch":2})"), target));
        (*assets)->hot_reload();
        auto current = (*assets)->json("/target.json");
        ASSERT_TRUE(current) << current.error().what();
        ASSERT_EQ(current->get_int("epoch"), 2);
        if (worker)
          ASSERT_TRUE((*assets)->queue_jsons(StringList{"/fence.json"}));
        gate->resume.release();
        released = true;
        if (worker) {
          // A single worker drains this fence only after returning from the old patch.
          ASSERT_TRUE(AssetWorkerTestAccess::drain(**assets));
        } else {
          auto retried = previous.get();
          ASSERT_TRUE(retried) << retried.error().what();
          EXPECT_EQ(*retried, *current);
        }
        auto cached = (*assets)->json("/target.json");
        ASSERT_TRUE(cached) << cached.error().what();
        EXPECT_EQ(*cached, *current);
        // An old task that acquired the new epoch would already have cached value 1.
        ASSERT_TRUE(File::write_file(String(R"({"value":2})"), dependency));
        auto nested = (*assets)->json("/dependency.json");
        ASSERT_TRUE(nested) << nested.error().what();
        EXPECT_EQ(nested->get_int("value"), 2);
      }
    }
  });

const GameTestRegistration reload_during_audio_post(
  "AssetsTest", "AudioPostClaimIsExclusiveAndCannotRemoveNewPost", [](GameTest& fixture) {
    auto source = fixture.root().assets();
    ASSERT_TRUE(source);
    auto ogg = (*source)->bytes("/sfx/interface/voice_on.ogg");
    ASSERT_TRUE(ogg) << ogg.error().what();
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    ASSERT_TRUE(File::write_file(String(R"([{"type":"audio","path":"/test.ogg"}])"),
        File::relative_to(*directory, "preload.config")));
    ASSERT_TRUE(File::write_file(**ogg, File::relative_to(*directory, "test.ogg")));
    Assets::Settings settings{};
    // Drive production worker selection and decoding on two threads at known boundaries.
    settings.worker_pool_size = 0;
    settings.audio_decompress_limit = 600;
    auto service = Assets::create(fixture.root().logger(), fixture.root().random(), fixture.root().logMap(),
                                  settings, {*directory});
    ASSERT_TRUE(service) << service.error().what();
    auto& assets = **service;
    {
      auto preload = AssetWorkerTestAccess::take_worker_task(assets);
      ASSERT_TRUE(preload);
      auto pcm = AssetWorkerTestAccess::decode_post(assets, *preload);
      ASSERT_TRUE(pcm) << pcm.error().what();
      ASSERT_TRUE(AssetWorkerTestAccess::finish_post(assets, std::move(*preload), std::move(pcm)));
      std::weak_ptr<Audio const> lifetime;
      {
        auto preloaded = assets.audio("/test.ogg");
        ASSERT_TRUE(preloaded);
        EXPECT_FALSE((*preloaded)->compressed());
        lifetime = *preloaded;
      }
      assets.clear_cache();
      EXPECT_FALSE(lifetime.expired()) << "Decompression discarded the preload persistence flag";
    }
    assets.hot_reload();
    auto original = assets.audio("/test.ogg");
    ASSERT_TRUE(original) << original.error().what();
    ASSERT_TRUE((*original)->compressed());

    std::binary_semaphore claimed{0}, resume{0};
    auto first = std::async(std::launch::async, [&]() -> std::expected<bool, StarException> {
      auto task = AssetWorkerTestAccess::take_worker_task(assets);
      if (!task)
        return std::unexpected(StarException("First worker did not receive an audio task"));
      auto pcm = AssetWorkerTestAccess::decode_post(assets, *task);
      claimed.release(); // Paused after real decompression, before publishing to the cache.
      if (!resume.try_acquire_for(std::chrono::seconds(30)))
        return std::unexpected(StarException("First worker was not resumed"));
      if (!pcm)
        return std::unexpected(std::move(pcm.error()));
      return AssetWorkerTestAccess::finish_post(assets, std::move(*task), std::move(pcm));
    });
    bool released = false;
    auto release = make_scope_exit([&] { if (!released) resume.release(); });
    ASSERT_TRUE(claimed.try_acquire_for(std::chrono::seconds(30)));
    // Worker two has no decompression to dispatch while worker one owns this post.
    EXPECT_FALSE(AssetWorkerTestAccess::take_worker_task(assets));
    auto readable = assets.audio("/test.ogg");
    ASSERT_TRUE(readable);
    EXPECT_EQ(*readable, *original);
    assets.hot_reload();
    auto replacement = assets.audio("/test.ogg");
    ASSERT_TRUE(replacement) << replacement.error().what();
    ASSERT_NE(*replacement, *original);
    ASSERT_TRUE((*replacement)->compressed());
    resume.release();
    released = true;
    auto finished = first.get();
    ASSERT_TRUE(finished) << finished.error().what();
    EXPECT_TRUE(*finished);
    auto cached = assets.audio("/test.ogg");
    ASSERT_TRUE(cached);
    EXPECT_EQ(*cached, *replacement);
    auto next = AssetWorkerTestAccess::take_worker_task(assets);
    ASSERT_TRUE(next) << "Obsolete completion removed the new epoch's pending post";
    auto pcm = AssetWorkerTestAccess::decode_post(assets, *next);
    ASSERT_TRUE(pcm) << pcm.error().what();
    auto posted = AssetWorkerTestAccess::finish_post(assets, std::move(*next), std::move(pcm));
    ASSERT_TRUE(posted) << posted.error().what();
    auto decoded = assets.audio("/test.ogg");
    ASSERT_TRUE(decoded);
    EXPECT_FALSE((*decoded)->compressed());
    EXPECT_TRUE((*original)->compressed());
    EXPECT_TRUE((*replacement)->compressed());
    EXPECT_FALSE(AssetWorkerTestAccess::take_worker_task(assets));

    assets.hot_reload();
    auto compressed = assets.audio("/test.ogg");
    ASSERT_TRUE(compressed);
    auto failed = AssetWorkerTestAccess::take_worker_task(assets);
    ASSERT_TRUE(failed);
    std::expected<bool, StarException> fallback_post;
    EXPECT_NONFATAL_FAILURE(
        fallback_post = AssetWorkerTestAccess::finish_post(assets, std::move(*failed),
            std::unexpected(AudioException("Test decoder failure"))),
        "Error was logged: Error post-processing asset: /test.ogg: Resource error: Test decoder failure");
    ASSERT_TRUE(fallback_post) << fallback_post.error().what();
    EXPECT_TRUE(*fallback_post);
    auto fallback = assets.audio("/test.ogg");
    ASSERT_TRUE(fallback);
    EXPECT_EQ(*fallback, *compressed);
    EXPECT_TRUE((*fallback)->compressed());
    EXPECT_FALSE(AssetWorkerTestAccess::take_worker_task(assets));
  });

const GameTestRegistration assetsFailureCacheReload(
  "AssetsTest", "QueuedFailureSurvivesCacheClearAndRecoversAfterReload", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    ASSERT_TRUE(File::write_file(String("[]"), File::relative_to(*directory, "preload.config")));
    auto path = File::relative_to(*directory, "failure.json");
    ASSERT_TRUE(File::write_file(String("{ invalid json"), path));
    ASSERT_TRUE(File::write_file(String(R"({"following":17})"), File::relative_to(*directory, "following.json")));
    Assets::Settings settings{};
    settings.worker_pool_size = 2;
    settings.asset_time_to_live = 600;
    Logger logger;
    auto assets = Assets::create(logger, fixture.root().random(), fixture.root().logMap(), settings, {*directory});
    ASSERT_TRUE(assets);
    ASSERT_TRUE((*assets)->queue_jsons(StringList{"/failure.json"}));
    ASSERT_TRUE(AssetWorkerTestAccess::drain(**assets));
    auto first = (*assets)->json("/failure.json");
    ASSERT_FALSE(first);
    EXPECT_TRUE(first.error().is<AssetException>());
    EXPECT_EQ(first.error().diagnostic_summary()->asset, "/failure.json");
    auto reason = first.error().diagnostic_summary()->reason;
    ASSERT_TRUE(File::write_file(String(R"({"recovered":42})"), path));
    (*assets)->clear_cache();
    ASSERT_TRUE((*assets)->queue_jsons(StringList{"/failure.json", "/following.json"}));
    ASSERT_TRUE(AssetWorkerTestAccess::drain(**assets));
    auto following = (*assets)->json("/following.json");
    ASSERT_TRUE(following);
    EXPECT_EQ(following->get_int("following"), 17);
    auto cached = (*assets)->json("/failure.json");
    ASSERT_FALSE(cached);
    EXPECT_EQ(cached.error().diagnostic_summary()->reason, reason);
    (*assets)->hot_reload();
    ASSERT_TRUE((*assets)->queue_jsons(StringList{"/failure.json"}));
    ASSERT_TRUE(AssetWorkerTestAccess::drain(**assets));
    auto recovered = (*assets)->json("/failure.json");
    ASSERT_TRUE(recovered);
    EXPECT_EQ(recovered->get_int("recovered"), 42);
  });

const GameTestRegistration recoverableDatabaseProvider(
  "AssetsTest", "RecoverableDatabaseFailureReportsProviderAndKeepsFallback", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    ASSERT_TRUE(File::write_file(String("[]"), File::relative_to(*directory, "preload.config")));
    ASSERT_TRUE(File::write_file(String(R"({"name":"dance_recovery","friendlyName":"Dance Recovery Provider"})"),
                                 File::relative_to(*directory, "_metadata")));
    ASSERT_TRUE(File::write_file(String("{}"), File::relative_to(*directory, "broken.dance")));
    ASSERT_TRUE(
      File::write_file(String(R"({"name":"assetmissing","states":[],"cycle":0,"cyclic":true,"duration":1,"steps":[]})"),
                       File::relative_to(*directory, "fallback.dance")));
    struct InvalidDance {
      std::string_view name, cycle, duration, steps;
    };
    constexpr InvalidDance invalid[] = {
      {"zero_cycle", "0", "1", "[{}]"}, {"negative_cycle", "-1", "1", "[{}]"},
      {"huge_cycle", "1e100", "1", "[{}]"}, {"negative_duration", "1", "-1", "[]"},
      {"huge_duration", "1", "1e100", "[]"}, {"short_step", "1", "1", "[[null,null,null,null,null]]"},
      {"bad_angle", "1", "1", R"([{"backArmRotation":"oops"}])"},
      {"huge_angle", "1", "1", R"([{"frontArmRotation":1e100}])"},
      {"huge_offset", "1", "1", R"([{"headOffset":[1e100,0]}])"},
    };
    for (auto const& entry : invalid) {
      auto config = Star::format(R"({{"name":"{}","states":["idle"],"cycle":{},"cyclic":true,"duration":{},"steps":{}}})",
                                 entry.name, entry.cycle, entry.duration, entry.steps);
      ASSERT_TRUE(File::write_file(String(std::move(config)), File::relative_to(*directory, String(entry.name) + ".dance")));
    }
    class Errors : public LogSink {
    public:
      Errors() { set_level(LogLevel::Error); }
      std::expected<void, StarException> log(std::string_view message, LogLevel) override {
        messages.append(message);
        return {};
      }
      std::string messages;
    };
    Logger logger;
    logger.remove_stdout_sink();
    auto errors = std::make_shared<Errors>();
    logger.add_sink(errors);
    Assets::Settings settings{};
    settings.worker_pool_size = 1;
    auto assets = Assets::create(logger, fixture.root().random(), fixture.root().logMap(), settings, {*directory});
    ASSERT_TRUE(assets);
    auto database = DanceDatabase::create(**assets, logger);
    ASSERT_TRUE(database);
    EXPECT_NE(errors->messages.find("/broken.dance"), std::string::npos);
    EXPECT_NE(errors->messages.find("JsonException"), std::string::npos);
    EXPECT_NE(errors->messages.find("Dance Recovery Provider"), std::string::npos);
    EXPECT_NE(errors->messages.find(directory->utf8()), std::string::npos);
    auto recovered = (*database)->get_dance("broken");
    ASSERT_TRUE(recovered);
    EXPECT_EQ((*recovered)->name, "assetmissing");
    EXPECT_FALSE((*recovered)->step_at(0));
    for (auto const& entry : invalid) {
      SCOPED_TRACE(entry.name);
      EXPECT_NE(errors->messages.find(Star::format("/{}.dance", entry.name)), std::string::npos);
      auto result = (*database)->get_dance(String(entry.name));
      ASSERT_TRUE(result);
      EXPECT_EQ(*result, *recovered);
    }
  });

const GameTestRegistration emote_matcher_differential(
  "AssetsTest", "EmoteMatcherDifferential", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    ASSERT_TRUE(File::write_file(String("[]"), File::relative_to(*directory, "preload.config")));
    std::string long_binding(160, 'x');
    long_binding.front() = 'y'; // Its 1.60 length bonus beats "x" one codepoint later.
    for (bool with_empty : {false, true}) {
      String config(std::string(R"json({"emoteBindings":{"notAnEmote":[],"Happy":[":)",":o","共享",")json")
          + long_binding + R"("],"Oh":[":O",":D","共享","中","x"])"
          + (with_empty ? R"(,"Sad":[""])" : "") + "}}");
      ASSERT_TRUE(File::write_file(config, File::relative_to(*directory, "emotes.config")));
      Logger logger;
      Assets::Settings settings{};
      settings.worker_pool_size = 1;
      auto assets = Assets::create(logger, fixture.root().random(), fixture.root().logMap(), settings, {*directory});
      ASSERT_TRUE(assets);
      auto processor = EmoteProcessor::create(*assets);
      ASSERT_TRUE(processor);
      auto json = (*assets)->json("/emotes.config");
      ASSERT_TRUE(json);
      auto groups = json->get_object("emoteBindings");
      ASSERT_TRUE(groups);
      for (String const& message : List<String>{"", "ABC", "Aa", "好ABC啊", "中:) :D :)", ":) :D :)",
               ":O", ":o", "共享", String(long_binding), String(std::string("a\0:D", 4)),
               String(std::string(":)\xff")), String(std::string("\xe4\xb8"))}) {
        auto result = (*processor)->detect_emotes(message);
        auto begin = message.begin();
        if (!begin) {
          ASSERT_FALSE(result);
          EXPECT_EQ(result.error().what(), begin.error().what());
          continue;
        }
        int balance = 0;
        for (auto it = *begin; it != message.end(); ++it) {
          auto point = *it;
          if (String::to_upper(point) != String::to_lower(point))
            balance += String::to_upper(point) == point ? 1 : -1;
        }
        auto expected = message.empty() ? HumanoidEmote::Idle
            : balance > 0 ? HumanoidEmote::Shouting : HumanoidEmote::Blabbering;
        float best = -1;
        for (auto const& [name, group] : *groups) {
          auto bindings = group.to_array();
          ASSERT_TRUE(bindings);
          for (auto const& binding : *bindings) {
            auto text = binding.to_string();
            ASSERT_TRUE(text);
            auto position = message.find(*text);
            ASSERT_TRUE(position);
            if (*position == std::string::npos) continue;
            auto length = text->length();
            ASSERT_TRUE(length);
            float score = *position + static_cast<float>(*length) * 0.01f;
            if (score > best) {
              auto emote = humanoid_emote_names->get_left(name);
              ASSERT_TRUE(emote);
              expected = emote->get();
              best = score;
            }
          }
        }
        ASSERT_TRUE(result);
        EXPECT_EQ(*result, expected);
      }
    }
  });

#pragma float_control(precise, on, push)
const GameTestRegistration dance_step_formats(
  "AssetsTest", "DanceStepFormatsAndFrameBoundaries", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    ASSERT_TRUE(File::write_file(String("[]"), File::relative_to(*directory, "preload.config")));
    ASSERT_TRUE(File::write_file(String(R"({
      "name":"poses","states":["idle"],"cycle":1,"cyclic":true,"duration":0.25,
      "steps":[
        ["idle.1",null,"idle.4",[1,2],null,[3,4]],
        {"frontArmFrame":"front","frontArmOffset":[5,6],"frontArmRotation":0.25},
        {"backArmRotation":-0.5},
        {"frontArmRotation":0.5,"backArmRotation":-0.25}
      ]
    })"), File::relative_to(*directory, "poses.dance")));
    Logger logger;
    Assets::Settings settings{};
    settings.worker_pool_size = 1;
    auto assets = Assets::create(logger, fixture.root().random(), fixture.root().logMap(), settings, {*directory});
    ASSERT_TRUE(assets);
    auto database = DanceDatabase::create(**assets, logger);
    ASSERT_TRUE(database);
    auto poses = (*database)->get_dance("poses");
    ASSERT_TRUE(poses);
    auto const& dance = **poses;
    ASSERT_EQ(dance.steps.size(), 4u);
    auto first = dance.step_at(-std::numeric_limits<float>::max());
    ASSERT_TRUE(first);
    EXPECT_EQ(first->get().body_frame, String("idle.1"));
    EXPECT_FALSE(first->get().front_arm_frame);
    EXPECT_EQ(first->get().back_arm_frame, String("idle.4"));
    EXPECT_EQ(first->get().head_offset, Vec2F(1, 2));
    EXPECT_EQ(first->get().front_arm_offset, Vec2F{});
    EXPECT_EQ(first->get().back_arm_offset, Vec2F(3, 4));
    EXPECT_EQ(first->get().front_arm_rotation, 0.f);
    EXPECT_EQ(first->get().back_arm_rotation, 0.f);
    auto front = dance.step_at(0.25f);
    ASSERT_TRUE(front);
    EXPECT_EQ(front->get().front_arm_frame, String("front"));
    EXPECT_EQ(front->get().front_arm_offset, Vec2F(5, 6));
    EXPECT_EQ(front->get().front_arm_rotation, 0.25f);
    EXPECT_EQ(front->get().back_arm_rotation, 0.25f);
    auto back = dance.step_at(0.5f);
    ASSERT_TRUE(back);
    EXPECT_EQ(back->get().front_arm_rotation, 0.f);
    EXPECT_EQ(back->get().back_arm_rotation, -0.5f);
    auto both = dance.step_at(1.75f);
    ASSERT_TRUE(both);
    EXPECT_EQ(both->get().front_arm_rotation, 0.5f);
    EXPECT_EQ(both->get().back_arm_rotation, -0.25f);
    auto wrapped = dance.step_at(1.f);
    ASSERT_TRUE(wrapped);
    EXPECT_EQ(wrapped->get().body_frame, first->get().body_frame);
    constexpr float nan = std::numeric_limits<float>::quiet_NaN();
    constexpr float infinity = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(dance.step_at(nan));
    EXPECT_FALSE(dance.step_at(infinity));

    Dance once;
    once.steps.resize(2);
    once.steps[1].body_frame = "last";
    once.cycle = std::numeric_limits<float>::max();
    for (float timer : {once.cycle * 0.75f, once.cycle}) {
      auto last = once.step_at(timer);
      ASSERT_TRUE(last);
      EXPECT_EQ(last->get().body_frame, String("last"));
    }
    once.cycle = 1;
    auto ended = once.step_at(std::numeric_limits<float>::max());
    ASSERT_TRUE(ended);
    EXPECT_EQ(ended->get().body_frame, String("last"));
    once.cycle = 0;
    EXPECT_FALSE(once.step_at(0));
  });
#pragma float_control(pop)

const GameTestRegistration memory_reader_lifetime(
  "AssetsTest", "MemoryReadersPinBytesAndPixelsAcrossReplacementAndDestruction", [](GameTest&) {
    auto const original = ByteArray::from_string(std::string_view("A\0\x80\xff" "xy", 6));
    for (bool use_image : {false, true}) {
      SCOPED_TRACE(use_image ? "image" : "bytes");
      std::shared_ptr<IODevice> reader;
      std::shared_ptr<IODevice> clone;
      std::shared_ptr<IODevice> latest;
      auto const replacement = use_image ? ByteArray::from_string("new") : ByteArray(3, 0);
      {
        MemoryAssetSource source("pinned");
        if (use_image) {
          Image pixels(2, 1, PixelFormat::RGB24);
          std::ranges::copy(std::as_bytes(original.span()), std::as_writable_bytes(pixels.span()).begin());
          source.set("/entry", pixels);
          std::ranges::fill(pixels.span(), 0);
        } else {
          source.set("/entry", original);
        }
        auto bytes = source.read("/entry");
        ASSERT_TRUE(bytes);
        EXPECT_EQ(*bytes, original);
        auto opened = source.open("/entry");
        ASSERT_TRUE(opened);
        reader = std::move(*opened);
        ASSERT_TRUE(reader->seek(2));
        auto copied = reader->clone();
        ASSERT_TRUE(copied);
        clone = std::move(*copied);

        for (unsigned i = 0; i != 64; ++i)
          source.set(Star::format("/other-{}", i), ByteArray{});
        if (use_image)
          source.set("/entry", replacement);
        else
          source.set("/entry", Image(1, 1, PixelFormat::RGB24));
        auto reopened = source.open("/entry");
        ASSERT_TRUE(reopened);
        latest = std::move(*reopened);
        // Leave the new image in its source until destruction; explicitly erase the new bytes.
        if (use_image) {
          ASSERT_TRUE(source.erase("/entry"));
          EXPECT_FALSE(source.open("/entry"));
        }
      }
      auto new_bytes = latest->read_bytes(replacement.size());
      ASSERT_TRUE(new_bytes);
      EXPECT_EQ(*new_bytes, replacement);
      auto tail = reader->read_bytes(4);
      ASSERT_TRUE(tail);
      EXPECT_EQ(*tail, original.substr(2, 4));
      reader.reset();
      auto cloned_tail = clone->read_bytes(4);
      ASSERT_TRUE(cloned_tail);
      EXPECT_EQ(*cloned_tail, original.substr(2, 4));
      ASSERT_TRUE(clone->seek(0));
      auto all = clone->read_bytes(6);
      ASSERT_TRUE(all);
      EXPECT_EQ(*all, original);
    }
  });

const GameTestRegistration memory_reader_seeks(
  "AssetsTest", "MemoryReaderSeeksClampFromCorrectOriginAndLeaveFailedAbsoluteUnchanged", [](GameTest&) {
    MemoryAssetSource source("seek");
    source.set("/entry", ByteArray::from_string("012345"));
    auto stream = source.open("/entry");
    ASSERT_TRUE(stream);
    auto& reader = **stream;
    ASSERT_TRUE(reader.seek(2));
    ASSERT_TRUE(reader.seek(1, IOSeek::End));
    EXPECT_EQ(reader.pos(), 5);
    EXPECT_FALSE(reader.seek(-1));
    EXPECT_FALSE(reader.seek(7));
    EXPECT_EQ(reader.pos(), 5);
    std::array<char, 3> buffer{'!', '!', '!'};
    EXPECT_EQ(reader.read(buffer), 1u);
    EXPECT_EQ((std::array<char, 3>{'5', '!', '!'}), buffer);
    EXPECT_EQ(reader.at_end(), true);
    EXPECT_EQ(reader.read(buffer), 0u);

    constexpr auto low = std::numeric_limits<StreamOffset>::min();
    constexpr auto high = std::numeric_limits<StreamOffset>::max();
    ASSERT_TRUE(reader.seek(low, IOSeek::Relative));
    EXPECT_EQ(reader.pos(), 0);
    EXPECT_EQ(reader.read({}), 0u);
    EXPECT_EQ(reader.pos(), 0);
    ASSERT_TRUE(reader.seek(high, IOSeek::Relative));
    EXPECT_EQ(reader.pos(), 6);
    ASSERT_TRUE(reader.seek(high, IOSeek::End));
    EXPECT_EQ(reader.pos(), 0);
    ASSERT_TRUE(reader.seek(low, IOSeek::End));
    EXPECT_EQ(reader.pos(), 6);

    source.set("/empty", ByteArray{});
    auto empty = source.open("/empty");
    ASSERT_TRUE(empty);
    ASSERT_TRUE((*empty)->seek(0, IOSeek::End));
    EXPECT_EQ((*empty)->at_end(), true);
    EXPECT_EQ((*empty)->read(buffer), 0u);
    EXPECT_FALSE((*empty)->seek(1));
  });
