import star.json;
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

#include "gtest/gtest.h"

import star.algorithm;
namespace Star {
struct AssetWorkerTestAccess {
  static std::expected<void, StarException> drain(Assets& assets) {
    std::unique_lock lock(assets.m_assetsMutex);
    if (!assets.m_assetsDone.wait_for(lock, std::chrono::seconds(60), [&] {
          return assets.m_queue.empty() || assets.m_workerError || assets.m_stopThreads;
        }))
      return std::unexpected(StarException("Asset queue regression did not drain"));
    if (assets.m_workerError)
      return std::unexpected(*assets.m_workerError);
    if (assets.m_stopThreads)
      return std::unexpected(StarException("Asset queue stopped before draining"));
    return {};
  }
};
}

using namespace Star;

namespace {
  std::filesystem::path fsPath(String const& path) {
    return std::filesystem::path(path.utf8());
  }

  struct PngIoFailureTag {
    static constexpr std::string_view name() { return "PngIoFailure"; }
  };
  using PngIoFailure = StarError<PngIoFailureTag, StarException>;

  class FailingPngNameBuffer : public Buffer {
  public:
    explicit FailingPngNameBuffer(ByteArray bytes) : Buffer(std::move(bytes)) {}
    [[nodiscard]] std::expected<String, StarException> deviceName() const override {
      return std::unexpected(PngIoFailure("Injected PNG device-name failure"));
    }
  };

  class FailingPngBuffer : public Buffer {
  public:
    FailingPngBuffer(ByteArray bytes, StreamOffset failAt) : Buffer(std::move(bytes)), m_failAt(failAt) {}

    [[nodiscard]] std::expected<void, StarException> readFull(std::span<char> data) override {
      auto ready = failIfRequested();
      if (!ready)
        return std::unexpected(std::move(ready.error()));
      return Buffer::readFull(data);
    }

    [[nodiscard]] std::expected<void, StarException> writeFull(std::span<char const> data) override {
      auto ready = failIfRequested();
      if (!ready)
        return std::unexpected(std::move(ready.error()));
      return Buffer::writeFull(data);
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

  EXPECT_EQ(
      AssetPath::relativeTo("/foo/bar/baz:baf?whoa?there", "thing:sub?directive"), "/foo/bar/thing:sub?directive");
});

const GameTestRegistration assetsSymlinkLoopIsNotFollowed("AssetsTest", "SymlinkLoopIsNotFollowed", [](GameTest& fixture) {
  // A link back to an ancestor must not make asset scanning fail with ELOOP.
  auto directory = File::temporaryDirectory(fixture.root().random());
  ASSERT_TRUE(directory);
  auto tempDir = std::move(*directory);
  auto finallyGuard = finally([&tempDir]() { EXPECT_TRUE(File::removeDirectoryRecursive(tempDir)); });

  std::error_code ec;
  std::filesystem::create_directories(fsPath(File::relativeTo(tempDir, "real")), ec);
  ASSERT_FALSE(ec) << ec.message();
  ASSERT_TRUE(File::writeFile(String("data"), File::relativeTo(File::relativeTo(tempDir, "real"), "asset.txt")));
  std::filesystem::create_directory_symlink(
      fsPath(File::relativeTo(tempDir, "real")), fsPath(File::relativeTo(tempDir, "link")), ec);
  if (ec)
    GTEST_SKIP() << "cannot create directory symlinks here: " << ec.message();
  std::filesystem::create_directory_symlink(fsPath(tempDir), fsPath(File::relativeTo(tempDir, "loop")), ec);
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
    (*source)->postRender();
    (*source)->tick(2);
    auto loop = (*source)->particles();
    ASSERT_TRUE(loop);
    EXPECT_EQ(*loop, (List<String>{"loop"}));
    (*source)->postRender();
    auto afterRender = (*source)->particles();
    ASSERT_TRUE(afterRender);
    EXPECT_EQ(*afterRender, List<String>{});
    (*source)->stop();
    (*source)->tick(0);
    auto final = (*source)->particles();
    ASSERT_FALSE(final);
    EXPECT_TRUE(final.error().is<JsonException>());
    auto repaired = definition.set("stop", JsonObject{{"particles", JsonArray{JsonArray{"stop"}}}});
    ASSERT_TRUE(repaired);
    auto replacement = EffectSource::create(fixture.root(), "private-cache-fixture", "normal", *repaired);
    ASSERT_TRUE(replacement);
    (*replacement)->postRender();
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
    auto written = buffer->writeFull({reinterpret_cast<char const*>(bytes.data()), bytes.size()});
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

const GameTestRegistration assetsFailedStartupJoinsWorkers("AssetsTest", "FailedStartupJoinsWorkers", [](GameTest& fixture) {
  auto directory = File::temporaryDirectory(fixture.root().random());
  ASSERT_TRUE(directory);
  auto tempDir = std::move(*directory);
  auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(tempDir)); });
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

  ASSERT_TRUE(File::writeFile(String("not json"), File::relativeTo(tempDir, "preload.config")));
  auto malformed = Assets::create(logger, fixture.root().random(), fixture.root().logMap(), settings, {tempDir});
  ASSERT_FALSE(malformed);
  EXPECT_TRUE(malformed.error().is<AssetException>());

  ASSERT_TRUE(File::writeFile(String("{}"), File::relativeTo(tempDir, "present.json")));
  ASSERT_TRUE(File::writeFile(
      String("[{\"type\":\"json\",\"path\":\"/present.json\"},{\"type\":\"json\",\"path\":\"/absent.json\"}]"),
      File::relativeTo(tempDir, "preload.config")));
  auto missing = Assets::create(logger, fixture.root().random(), fixture.root().logMap(), settings, {tempDir});
  ASSERT_FALSE(missing);
  EXPECT_TRUE(missing.error().is<AssetException>());

  ASSERT_TRUE(File::writeFile(String("[{\"type\":\"image\",\"path\":\"relative.png\"}]"),
      File::relativeTo(tempDir, "preload.config")));
  auto invalid = Assets::create(logger, fixture.root().random(), fixture.root().logMap(), settings, {tempDir});
  ASSERT_FALSE(invalid);
  EXPECT_TRUE(invalid.error().is<AssetException>());
  ASSERT_TRUE(File::writeFile(String("[]"), File::relativeTo(tempDir, "preload.config")));
  auto valid = Assets::create(logger, fixture.root().random(), fixture.root().logMap(), settings, {tempDir});
  ASSERT_TRUE(valid);
  auto recovered = (*valid)->json("/present.json");
  ASSERT_TRUE(recovered);
  EXPECT_EQ(*recovered, Json(JsonObject{}));
});

const GameTestRegistration assetsShutdownWithQueuedImageDependencies("AssetsTest", "ShutdownWithQueuedImageDependencies", [](GameTest& fixture) {
  auto directory = File::temporaryDirectory(fixture.root().random());
  ASSERT_TRUE(directory);
  auto tempDir = std::move(*directory);
  auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(tempDir)); });
  ASSERT_TRUE(File::writeFile(String("[]"), File::relativeTo(tempDir, "preload.config")));
  auto bytes = pngTestBytes(fixture.root().logger());
  ASSERT_TRUE(bytes);
  ASSERT_TRUE(File::writeFile(*bytes, File::relativeTo(tempDir, "sample.png")));
  Assets::Settings settings{};
  settings.workerPoolSize = 4;
  settings.assetTimeToLive = 10;
  for (unsigned attempt = 0; attempt < 16; ++attempt) {
    auto assets = Assets::create(fixture.root().logger(), fixture.root().random(), fixture.root().logMap(), settings, {tempDir});
    ASSERT_TRUE(assets);
    ASSERT_TRUE((*assets)->queueImages(StringList{"/sample.png?flipx", "/sample.png?flipy", "/sample.png"}));
  }
});

const GameTestRegistration
  assetsQueuedLuaPatches("AssetsTest", "QueuedLuaPatchErrorsDoNotDiscardOtherResults", [](GameTest& fixture) {
    auto directory = File::temporaryDirectory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(*directory)); });
    ASSERT_TRUE(File::writeFile(String("[]"), File::relativeTo(*directory, "preload.config")));
    ASSERT_TRUE(File::writeFile(String("{}"), File::relativeTo(*directory, "broken.json")));
    ASSERT_TRUE(File::writeFile(String("function patch() error('deliberate patch failure') end"),
                                File::relativeTo(*directory, "broken.json.patch.lua")));
    StringList paths{"/broken.json"};
    for (unsigned i = 0; i != 32; ++i) {
      auto name = strf("queued{}.json", i);
      ASSERT_TRUE(File::writeFile(String(strf("{{\"index\":{}}}", i)), File::relativeTo(*directory, name)));
      ASSERT_TRUE(File::writeFile(String(R"lua(function patch(asset)
      local garbage = {}
      for i = 1, 128 do
        local node = {value = i}
        node.self = node
        garbage[i] = node
      end
      asset.result = asset.index + garbage[128].value
      return asset
    end)lua"),
                                  File::relativeTo(*directory, name + ".patch.lua")));
      paths.append("/" + name);
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
  auto directory = File::temporaryDirectory(fixture.root().random());
  ASSERT_TRUE(directory);
  auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(*directory)); });
  ASSERT_TRUE(File::writeFile(String("[]"), File::relativeTo(*directory, "preload.config")));
  StringList paths;
  JsonArray targets;
  for (unsigned i = 0; i <= 32; ++i) {
    auto name = strf("shared{}.json", i);
    ASSERT_TRUE(File::writeFile(String(strf("{{\"index\":{}}}", i)), File::relativeTo(*directory, name)));
    paths.append("/" + name);
    targets.emplace_back(paths.back());
  }
  Json patchList = JsonArray{JsonObject{{"paths", std::move(targets)}, {"patches", JsonArray{"/shared.lua"}}}};
  auto encoded = patchList.repr();
  ASSERT_TRUE(encoded);
  // Patch lists attach to already registered targets in source scan order.
  ASSERT_TRUE(File::writeFile(*encoded, File::relativeTo(*directory, "zz-shared.patchlist")));
  ASSERT_TRUE(File::writeFile(String(R"lua(counter = 0
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
)lua"), File::relativeTo(*directory, "shared.lua")));
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
        batch.append(paths[i]);
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
