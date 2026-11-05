import std;
import star.algorithm;
import star.assets;
import star.buffer;
import star.byte_array;
import star.directory_asset_source;
import star.file;
import star.format;
import star.game_test;
import star.image;
import star.json;
import star.list;
import star.logging;
import star.packed_asset_source;
import star.root;
import star.string;
import star.vector;

#include "gtest/gtest.h"

using namespace Star;

namespace {
std::filesystem::path patchFsPath(String const& path) { return std::filesystem::path(path.utf8()); }

std::expected<void, StarException> writeJsonPatchFixture(String const& directory, String const& suffix,
                                                         String const& patch) {
  for (auto const& [name, contents] : std::array{
         std::pair{String("preload.config"), String("[]")},
         std::pair{String("target.json"), String(R"({"base":true})")}, std::pair{String("target.json") + suffix, patch},
         std::pair{String("target.json.patch1"), String(R"({"later":true})")}}) {
    auto written = File::writeFile(contents, File::relativeTo(directory, name));
    if (!written)
      return std::unexpected(std::move(written.error()));
  }
  return {};
}

std::expected<std::shared_ptr<Assets>, StarException> patchTestAssets(GameTest& fixture, String const& directory,
                                                                      Logger& logger) {
  Assets::Settings settings{};
  settings.workerPoolSize = 1;
  settings.assetTimeToLive = 600;
  return Assets::create(logger, fixture.root().random(), fixture.root().logMap(), settings, {directory});
}

std::expected<void, StarException> writeImagePatchFixture(GameTest& fixture, String const& directory,
                                                          String const& patch) {
  auto written = File::writeFile(String("[]"), File::relativeTo(directory, "preload.config"));
  if (!written)
    return std::unexpected(std::move(written.error()));
  Image image(2, 1, PixelFormat::RGBA32);
  auto red = image.set(0, 0, Vec4B{255, 0, 0, 255});
  if (!red)
    return std::unexpected(std::move(red.error()));
  auto blue = image.set(1, 0, Vec4B{0, 0, 255, 255});
  if (!blue)
    return std::unexpected(std::move(blue.error()));
  auto buffer = std::make_shared<Buffer>();
  auto encoded = image.writePng(fixture.root().logger(), buffer);
  if (!encoded)
    return std::unexpected(std::move(encoded.error()));
  written = File::writeFile(buffer->takeData(), File::relativeTo(directory, "target.png"));
  if (!written)
    return std::unexpected(std::move(written.error()));
  written = File::writeFile(patch, File::relativeTo(directory, "target.png.patch.lua"));
  if (!written)
    return std::unexpected(std::move(written.error()));
  written = File::writeFile(String("function patch(image) return image:process('?flipx') end"),
                            File::relativeTo(directory, "later.lua"));
  if (!written)
    return std::unexpected(std::move(written.error()));
  // The patch list sorts after the target and its automatically indexed Lua patch.
  return File::writeFile(String(R"([{"paths":["/target.png"],"patches":["/later.lua"]}])"),
                         File::relativeTo(directory, "zz-later.patchlist"));
}

std::expected<void, StarException> writePatchProvider(String const& directory,
    std::initializer_list<std::pair<String, String>> files) {
  auto created = File::makeDirectory(directory);
  if (!created)
    return std::unexpected(std::move(created.error()));
  for (auto const& [path, contents] : files) {
    auto written = File::writeFile(contents, File::relativeTo(directory, path));
    if (!written)
      return std::unexpected(std::move(written.error()));
  }
  return {};
}
}// namespace

const GameTestRegistration assetsRemovedLooseJsonPatches(
  "AssetsPatchReadTest", "RemovedLooseJsonPatchesKeepBaseAndLaterPatch", [](GameTest& fixture) {
    for (String const& suffix : {String(".patch"), String(".patch.lua")}) {
      SCOPED_TRACE(suffix.utf8());
      auto directory = File::temporaryDirectory(fixture.root().random());
      ASSERT_TRUE(directory);
      auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(*directory)); });
      auto patch = suffix == ".patch" ? String(R"({"removed":true})")
                                      : String("function patch(data) data.removed = true return data end");
      ASSERT_TRUE(writeJsonPatchFixture(*directory, suffix, patch));
      Logger logger;
      auto assets = patchTestAssets(fixture, *directory, logger);
      ASSERT_TRUE(assets) << assets.error().what();
      // Removal happens only after Assets has indexed the genuine loose patch.
      ASSERT_TRUE(File::remove(File::relativeTo(*directory, "target.json" + suffix)));
      auto loaded = (*assets)->json("/target.json");
      ASSERT_TRUE(loaded) << loaded.error().what();
      EXPECT_EQ(*loaded, Json(JsonObject{{"base", true}, {"later", true}}));
    }
  });

const GameTestRegistration
  assetsNestedPatchGroups("AssetsPatchReadTest", "NestedGroupsCommitOnlySuccessfulGroups", [](GameTest& fixture) {
    auto directory = File::temporaryDirectory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(*directory)); });
    ASSERT_TRUE(writeJsonPatchFixture(*directory, ".patch", R"([
      {"op":"add","path":"/before","value":true},
      [
        {"op":"add","path":"/discarded","value":true},
        {"op":"test","path":"/base","value":false}
      ],
      [
        {"op":"add","path":"/committed","value":true},
        {"op":"test","path":"/base","value":true}
      ],
      {"op":"add","path":"/after","value":true}
    ])"));
    Logger logger;
    auto assets = patchTestAssets(fixture, *directory, logger);
    ASSERT_TRUE(assets) << assets.error().what();
    auto loaded = (*assets)->json("/target.json");
    ASSERT_TRUE(loaded) << loaded.error().what();
    EXPECT_EQ(
      *loaded,
      Json(JsonObject{{"base", true}, {"before", true}, {"committed", true}, {"after", true}, {"later", true}}));
  });

const GameTestRegistration assetsRemovedCachedLuaPatch(
  "AssetsPatchReadTest", "RemovedCachedLuaPatchIsNotInvokedForAnotherTarget", [](GameTest& fixture) {
    auto directory = File::temporaryDirectory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(*directory)); });
    ASSERT_TRUE(
      writeJsonPatchFixture(*directory, ".patch.lua", "function patch(data) data.removed = true return data end"));
    ASSERT_TRUE(File::writeFile(String(R"({"warmup":true})"), File::relativeTo(*directory, "warmup.json")));
    ASSERT_TRUE(File::writeFile(String(R"([{"paths":["/warmup.json"],"patches":["/target.json.patch.lua"]}])"),
                                File::relativeTo(*directory, "zz-warmup.patchlist")));
    Logger logger;
    auto assets = patchTestAssets(fixture, *directory, logger);
    ASSERT_TRUE(assets) << assets.error().what();
    auto warmup = (*assets)->json("/warmup.json");
    ASSERT_TRUE(warmup) << warmup.error().what();
    EXPECT_EQ(*warmup, Json(JsonObject{{"warmup", true}, {"removed", true}}));
    ASSERT_TRUE(File::remove(File::relativeTo(*directory, "target.json.patch.lua")));
    // This target is uncached, while the same source/path Lua context is already cached.
    auto loaded = (*assets)->json("/target.json");
    ASSERT_TRUE(loaded) << loaded.error().what();
    EXPECT_EQ(*loaded, Json(JsonObject{{"base", true}, {"later", true}}));
  });

const GameTestRegistration assetsPatchSyntaxAndRuntimeErrors(
  "AssetsPatchReadTest", "JsonAndLuaSyntaxAndLuaRuntimeErrorsRemainFailures", [](GameTest& fixture) {
    for (auto const& [suffix, patch] : std::array{
           std::pair{String(".patch"), String("{ not valid json")},
           std::pair{String(".patch.lua"), String("function patch(data) this is not Lua end")},
           std::pair{String(".patch.lua"), String("function patch(data) error('deliberate patch failure') end")}}) {
      SCOPED_TRACE(patch.utf8());
      auto directory = File::temporaryDirectory(fixture.root().random());
      ASSERT_TRUE(directory);
      auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(*directory)); });
      ASSERT_TRUE(writeJsonPatchFixture(*directory, suffix, patch));
      Logger logger;
      auto assets = patchTestAssets(fixture, *directory, logger);
      ASSERT_TRUE(assets) << assets.error().what();
      auto loaded = (*assets)->json("/target.json");
      ASSERT_FALSE(loaded);
      EXPECT_TRUE(loaded.error().is<AssetException>());
    }
  });

const GameTestRegistration assetsPatchReplacedWithDirectory(
  "AssetsPatchReadTest", "PatchReplacedWithDirectoryRemainsFailure", [](GameTest& fixture) {
    for (String const& suffix : {String(".patch"), String(".patch.lua")}) {
      SCOPED_TRACE(suffix.utf8());
      auto directory = File::temporaryDirectory(fixture.root().random());
      ASSERT_TRUE(directory);
      auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(*directory)); });
      ASSERT_TRUE(writeJsonPatchFixture(
        *directory, suffix, suffix == ".patch" ? String("{}") : String("function patch(data) return data end")));
      Logger logger;
      auto assets = patchTestAssets(fixture, *directory, logger);
      ASSERT_TRUE(assets) << assets.error().what();
      auto patchPath = File::relativeTo(*directory, "target.json" + suffix);
      ASSERT_TRUE(File::remove(patchPath));
      std::error_code ec;
      ASSERT_TRUE(std::filesystem::create_directory(patchFsPath(patchPath), ec)) << ec.message();
      auto loaded = (*assets)->json("/target.json");
      ASSERT_FALSE(loaded);
      EXPECT_TRUE(loaded.error().is<AssetException>());
    }
  });

const GameTestRegistration assetsPatchReplacedWithBrokenSymlink(
  "AssetsPatchReadTest", "PatchReplacedWithBrokenSymlinkRemainsFailure", [](GameTest& fixture) {
    for (String const& suffix : {String(".patch"), String(".patch.lua")}) {
      SCOPED_TRACE(suffix.utf8());
      auto directory = File::temporaryDirectory(fixture.root().random());
      ASSERT_TRUE(directory);
      auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(*directory)); });
      ASSERT_TRUE(writeJsonPatchFixture(
        *directory, suffix, suffix == ".patch" ? String("{}") : String("function patch(data) return data end")));
      Logger logger;
      auto assets = patchTestAssets(fixture, *directory, logger);
      ASSERT_TRUE(assets) << assets.error().what();
      auto patchPath = File::relativeTo(*directory, "target.json" + suffix);
      ASSERT_TRUE(File::remove(patchPath));
      std::error_code ec;
      std::filesystem::create_symlink(patchFsPath(File::relativeTo(*directory, "absent-link-target")),
                                      patchFsPath(patchPath), ec);
      if (ec)
        GTEST_SKIP() << "cannot create symlinks here: " << ec.message();
      auto status = std::filesystem::symlink_status(patchFsPath(patchPath), ec);
      ASSERT_FALSE(ec) << ec.message();
      ASSERT_TRUE(std::filesystem::is_symlink(status));
      auto loaded = (*assets)->json("/target.json");
      ASSERT_FALSE(loaded);
      EXPECT_TRUE(loaded.error().is<AssetException>());
    }
  });

const GameTestRegistration
  assetsUnreadablePatch("AssetsPatchReadTest", "UnreadablePatchRemainsFailure", [](GameTest& fixture) {
    for (String const& suffix : {String(".patch"), String(".patch.lua")}) {
      SCOPED_TRACE(suffix.utf8());
      auto directory = File::temporaryDirectory(fixture.root().random());
      ASSERT_TRUE(directory);
      auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(*directory)); });
      ASSERT_TRUE(writeJsonPatchFixture(
        *directory, suffix, suffix == ".patch" ? String("{}") : String("function patch(data) return data end")));
      Logger logger;
      auto assets = patchTestAssets(fixture, *directory, logger);
      ASSERT_TRUE(assets) << assets.error().what();
      auto path = patchFsPath(File::relativeTo(*directory, "target.json" + suffix));
      std::error_code ec;
      auto original = std::filesystem::status(path, ec).permissions();
      ASSERT_FALSE(ec) << ec.message();
      auto restorePermissions = finally([&] {
        std::error_code restoreError;
        std::filesystem::permissions(path, original, restoreError);
        EXPECT_FALSE(restoreError) << restoreError.message();
      });
      std::filesystem::permissions(path, std::filesystem::perms::none, ec);
      if (ec)
        GTEST_SKIP() << "cannot remove file read permissions here: " << ec.message();
      // Root/CAP_DAC_OVERRIDE and some filesystems can still read a mode-000 file.
      if (File::readFile(File::relativeTo(*directory, "target.json" + suffix)))
        GTEST_SKIP() << "this process can still read a mode-000 patch";
      auto loaded = (*assets)->json("/target.json");
      ASSERT_FALSE(loaded);
      EXPECT_TRUE(loaded.error().is<AssetException>());
    }
  });

const GameTestRegistration assetsRemovedLooseImageLuaPatch(
  "AssetsPatchReadTest", "RemovedLooseImageLuaPatchKeepsPixelsAndLaterPatch", [](GameTest& fixture) {
    auto directory = File::temporaryDirectory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(*directory)); });
    ASSERT_TRUE(
      writeImagePatchFixture(fixture, *directory, "function patch(image) return image:process('?flipx') end"));
    Logger logger;
    auto assets = patchTestAssets(fixture, *directory, logger);
    ASSERT_TRUE(assets) << assets.error().what();
    ASSERT_TRUE(File::remove(File::relativeTo(*directory, "target.png.patch.lua")));
    auto loaded = (*assets)->image("/target.png");
    ASSERT_TRUE(loaded) << loaded.error().what();
    EXPECT_EQ((*loaded)->size(), Vec2U(2, 1));
    auto left = (*loaded)->get(0, 0);
    auto right = (*loaded)->get(1, 0);
    ASSERT_TRUE(left);
    ASSERT_TRUE(right);
    EXPECT_EQ(*left, (Vec4B{0, 0, 255, 255}));
    EXPECT_EQ(*right, (Vec4B{255, 0, 0, 255}));
  });

const GameTestRegistration assetsImageLuaPatchErrors(
  "AssetsPatchReadTest", "ImageLuaSyntaxAndRuntimeErrorsRemainFailures", [](GameTest& fixture) {
    for (String const& patch : {String("function patch(image) this is not Lua end"),
                                String("function patch(image) error('deliberate image patch failure') end")}) {
      SCOPED_TRACE(patch.utf8());
      auto directory = File::temporaryDirectory(fixture.root().random());
      ASSERT_TRUE(directory);
      auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(*directory)); });
      ASSERT_TRUE(writeImagePatchFixture(fixture, *directory, patch));
      Logger logger;
      auto assets = patchTestAssets(fixture, *directory, logger);
      ASSERT_TRUE(assets) << assets.error().what();
      auto loaded = (*assets)->image("/target.png");
      ASSERT_FALSE(loaded);
    }
  });

const GameTestRegistration assetsLuaPatchProvider(
  "AssetsPatchReadTest", "LuaPatchFailureUsesExecutingProviderInsteadOfOverriddenPath", [](GameTest& fixture) {
    for (bool image : {false, true}) {
      for (bool syntax : {false, true}) {
        SCOPED_TRACE(image ? "image" : "json");
        SCOPED_TRACE(syntax ? "compile" : "invoke");
        auto target = File::temporaryDirectory(fixture.root().random());
        auto failing = File::temporaryDirectory(fixture.root().random());
        auto shadow = File::temporaryDirectory(fixture.root().random());
        ASSERT_TRUE(target);
        ASSERT_TRUE(failing);
        ASSERT_TRUE(shadow);
        auto cleanup = finally([&] {
          EXPECT_TRUE(File::removeDirectoryRecursive(*target));
          EXPECT_TRUE(File::removeDirectoryRecursive(*failing));
          EXPECT_TRUE(File::removeDirectoryRecursive(*shadow));
        });
        ASSERT_TRUE(File::writeFile(String(R"({"name":"diagnostic_target"})"), File::relativeTo(*target, "_metadata")));
        ASSERT_TRUE(File::writeFile(
          String(R"({"name":"diagnostic_failure","friendlyName":"Failing patch provider"})"),
          File::relativeTo(*failing, "_metadata")));
        ASSERT_TRUE(File::writeFile(
          String(R"({"name":"diagnostic_shadow","friendlyName":"Unexecuted shadow provider"})"),
          File::relativeTo(*shadow, "_metadata")));
        String pass = "function patch(value) return value end";
        if (image) {
          ASSERT_TRUE(writeImagePatchFixture(fixture, *target, pass));
        } else {
          ASSERT_TRUE(writeJsonPatchFixture(*target, ".patch.lua", pass));
        }
        String patchFile = image ? String("target.png.patch.lua") : String("target.json.patch.lua");
        String patchPath = "/" + patchFile;
        String failure = syntax ? String("function patch(value) invalid syntax end")
                                : String("function patch(value) error('provider_runtime_failure') end");
        ASSERT_TRUE(File::writeFile(failure, File::relativeTo(*failing, patchFile)));
        ASSERT_TRUE(File::writeFile(pass, File::relativeTo(*shadow, patchFile)));
        String targetPath = image ? String("/target.png") : String("/target.json");
        ASSERT_TRUE(File::writeFile(String("{}"), File::relativeTo(*target, "caller.json")));
        ASSERT_TRUE(File::writeFile(
          image ? String("function patch(value) assets.image('/target.png') return value end")
                : String("function patch(value) assets.json('/target.json') return value end"),
          File::relativeTo(*target, "caller.json.patch.lua")));
        Logger logger;
        Assets::Settings settings{};
        settings.workerPoolSize = 1;
        settings.assetTimeToLive = 600;
        auto assets = Assets::create(logger, fixture.root().random(), fixture.root().logMap(), settings,
                                     {*target, *failing, *shadow});
        ASSERT_TRUE(assets) << assets.error().what();
        auto descriptor = (*assets)->assetDescriptor(patchPath);
        ASSERT_TRUE(descriptor);
        ASSERT_TRUE(*descriptor);
        auto indexedSource = (*assets)->assetSourcePath((*descriptor)->source);
        ASSERT_TRUE(indexedSource);
        EXPECT_EQ(*indexedSource, *shadow);
        auto checkFailure = [&](auto const& loaded) {
          ASSERT_FALSE(loaded);
          auto const& summary = loaded.error().diagnosticSummary();
          ASSERT_TRUE(summary);
          EXPECT_EQ(summary->kind, syntax ? ExceptionSummaryKind::PatchSyntax : ExceptionSummaryKind::PatchRuntime);
          EXPECT_EQ(summary->asset, targetPath.utf8());
          EXPECT_EQ(summary->patch, patchPath.utf8());
          EXPECT_NE(summary->patchProvider.find(failing->utf8()), std::string::npos);
          EXPECT_NE(summary->patchProvider.find("diagnostic_failure"), std::string::npos);
          EXPECT_EQ(summary->patchProvider.find(shadow->utf8()), std::string::npos);
          if (!syntax)
            EXPECT_NE(summary->reason.find("provider_runtime_failure"), std::string::npos);
          std::size_t patchLocations = 0;
          for (auto const& location : summary->locations) {
            if (location.source == patchPath.utf8()) {
              ++patchLocations;
              if (!syntax)
                EXPECT_EQ(location.line, 1);
              EXPECT_EQ(location.provider, summary->patchProvider);
            }
          }
          EXPECT_EQ(patchLocations, 1u);
          std::string_view diagnostic = loaded.error().what();
          EXPECT_NE(diagnostic.find(failing->utf8()), std::string_view::npos);
          EXPECT_NE(diagnostic.find("diagnostic_failure"), std::string_view::npos);
          EXPECT_NE(diagnostic.find("Failing patch provider"), std::string_view::npos);
          EXPECT_NE(diagnostic.find(patchPath.utf8()), std::string_view::npos);
          EXPECT_EQ(diagnostic.find("Unexecuted shadow provider"), std::string_view::npos);
          if (!syntax)
            EXPECT_NE(diagnostic.find("provider_runtime_failure"), std::string_view::npos);
        };
        if (image)
          checkFailure((*assets)->image("/target.png"));
        else
          checkFailure((*assets)->json("/target.json"));
        // The real asset callback must carry the failing inner patch through
        // Lua's C bridge without replacing it with the outer caller's patch.
        checkFailure((*assets)->json("/caller.json"));
      }
    }
  });

const GameTestRegistration assetsMissingResourceSummary(
  "AssetsPatchReadTest", "MissingResourceKeepsUnderlyingReasonInStructuredSummary", [](GameTest& fixture) {
    auto directory = File::temporaryDirectory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(*directory)); });
    ASSERT_TRUE(File::writeFile(String("[]"), File::relativeTo(*directory, "preload.config")));
    Logger logger;
    auto assets = patchTestAssets(fixture, *directory, logger);
    ASSERT_TRUE(assets) << assets.error().what();
    auto loaded = (*assets)->json("/missing-summary.json");
    ASSERT_FALSE(loaded);
    auto const& summary = loaded.error().diagnosticSummary();
    ASSERT_TRUE(summary);
    EXPECT_EQ(summary->kind, ExceptionSummaryKind::Resource);
    EXPECT_EQ(summary->asset, "/missing-summary.json");
    EXPECT_NE(summary->reason.find("/missing-summary.json"), std::string::npos);
    EXPECT_TRUE(summary->patch.empty());
    EXPECT_TRUE(summary->patchProvider.empty());
  });

const GameTestRegistration assetsSourceSpecificPatchReplacement(
  "AssetsPatchReadTest", "SourceSpecificReplacementPreservesOrderAndSameContextReads", [](GameTest& fixture) {
    auto directory = File::temporaryDirectory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(*directory)); });
    auto base = File::relativeTo(*directory, "base");
    auto original = File::relativeTo(*directory, "original");
    auto shadow = File::relativeTo(*directory, "shadow");
    auto repair = File::relativeTo(*directory, "repair");
    ASSERT_TRUE(writePatchProvider(base, {
      {"preload.config", "[]"}, {"target.json", R"({"trace":[]})"},
      {"target.json.patch", R"([{"op":"add","path":"/trace/-","value":"before"}])"}
    }));
    ASSERT_TRUE(writePatchProvider(original, {
      {"_metadata", R"({"name":"original_provider"})"},
      {"shared.patch", R"([{"op":"add","path":"/trace/-","value":"original"}])"},
      {"zz.patchlist", R"([{"paths":["/target.json"],"patches":["/shared.patch","/shared.patch"]}])"}
    }));
    ASSERT_TRUE(writePatchProvider(shadow, {
      {"_metadata", R"({"name":"shadow_provider"})"},
      {"shared.patch", R"([{"op":"add","path":"/trace/-","value":"shadow"}])"},
      {"later.lua", R"(function patch(data)
        assert(assets.patchReplace == nil and assets.loadScriptReplace == nil and assets.patch == nil and assets.add == nil and assets.erase == nil)
        table.insert(data.trace, "later")
        return data
      end)"},
      {"zz.patchlist", R"([{"paths":["/target.json"],"patches":["/shared.patch","/later.lua"]}])"}
    }));
    ASSERT_TRUE(writePatchProvider(repair, {
      {"_metadata", R"({"name":"repair_provider","scripts":{"postLoad":["/repair.lua"]}})"},
      {"repair.lua", R"(
        local original, shadow
        for path, metadata in pairs(assets.sourcePaths(true)) do
          if metadata.name == "original_provider" then original = path end
          if metadata.name == "shadow_provider" then shadow = path end
        end
        assert(original and shadow)
        local originalBytes = assets.sourceBytes(original, "/shared.patch")
        assert(originalBytes:find('"original"', 1, true))
        assert(assets.sourceBytes(shadow, "/shared.patch") == assets.bytes("/shared.patch"))
        assert(originalBytes ~= assets.bytes("/shared.patch"))
        local function trace() return table.concat(assets.json("/target.json").trace, ",") end
        local before = "before,original,original,shadow,later"
        assert(trace() == before)
        assets.add("/repaired.patch", '[{"op":"add","path":"/trace/-","value":"repaired"}]')
        assert(not assets.patchReplace("/absent.json", "/shared.patch", original, "/repaired.patch"))
        assert(not assets.patchReplace("/target.json", "/absent.patch", original, "/repaired.patch"))
        assert(not assets.patchReplace("/target.json", "/shared.patch", "unknown-provider", "/repaired.patch"))
        assert(not assets.patchReplace("/target.json", "/shared.patch", original, "/absent.patch"))
        assert(trace() == before)
        local ok, message = pcall(assets.sourceBytes, "unknown-provider", "/shared.patch")
        assert(not ok and message:find("unknown-provider", 1, true) and message:find("/shared.patch", 1, true))
        ok, message = pcall(assets.sourceBytes, original, "/../shadow/shared.patch")
        assert(not ok and message:find(original, 1, true) and message:find("/../shadow/shared.patch", 1, true))
        ok, message = pcall(assets.sourceBytes, original, "/SHARED.patch")
        assert(not ok and message:find(original, 1, true) and message:find("/SHARED.patch", 1, true))
        assert(assets.patchReplace("/target.json", "/shared.patch", original, "/repaired.patch"))
        assert(trace() == "before,repaired,repaired,shadow,later")
        assert(assets.sourceBytes(original, "/shared.patch") == originalBytes)
        assert(not assets.patchReplace("/target.json", "/shared.patch", original, "/repaired.patch"))
        assets.add("/same-context.json", assets.json("/target.json"))
      )"}
    }));
    Logger logger;
    Assets::Settings settings{};
    settings.workerPoolSize = 1;
    settings.assetTimeToLive = 600;
    auto assets = Assets::create(logger, fixture.root().random(), fixture.root().logMap(), settings,
        {base, original, shadow, repair});
    ASSERT_TRUE(assets) << assets.error().what();
    auto expected = Json::parseJson(R"({"trace":["before","repaired","repaired","shadow","later"]})");
    ASSERT_TRUE(expected);
    auto observed = (*assets)->json("/same-context.json");
    ASSERT_TRUE(observed) << observed.error().what();
    EXPECT_EQ(*observed, *expected);
    auto loaded = (*assets)->json("/target.json");
    ASSERT_TRUE(loaded) << loaded.error().what();
    EXPECT_EQ(*loaded, *expected);
  });

const GameTestRegistration assetsReplacePreviouslyFailedPatch(
  "AssetsPatchReadTest", "SourceSpecificReplacementInvalidatesEarlierFailure", [](GameTest& fixture) {
    auto directory = File::temporaryDirectory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(*directory)); });
    auto base = File::relativeTo(*directory, "base");
    auto repair = File::relativeTo(*directory, "repair");
    ASSERT_TRUE(writePatchProvider(base, {
      {"preload.config", "[]"}, {"target.json", R"({"base":true})"},
      {"target.json.patch.lua", "function patch(data) error('replace_cached_failure') end"}
    }));
    ASSERT_TRUE(writePatchProvider(repair, {
      {"_metadata", R"({"scripts":{"postLoad":["/repair.lua"]}})"},
      {"repair.lua", R"(
        local original = assets.origin("/target.json.patch.lua")
        assert(not pcall(assets.json, "/target.json"))
        assets.add("/replacement.lua", "function patch(data) data.repaired = true return data end")
        assert(assets.patchReplace("/target.json", "/target.json.patch.lua", original, "/replacement.lua"))
        local data = assets.json("/target.json")
        assert(data.base and data.repaired)
        assets.add("/same-context.json", data)
      )"}
    }));
    Logger logger;
    Assets::Settings settings{};
    settings.workerPoolSize = 1;
    auto assets = Assets::create(logger, fixture.root().random(), fixture.root().logMap(), settings, {base, repair});
    ASSERT_TRUE(assets) << assets.error().what();
    auto observed = (*assets)->json("/same-context.json");
    ASSERT_TRUE(observed) << observed.error().what();
    EXPECT_EQ(*observed, Json(JsonObject{{"base", true}, {"repaired", true}}));
  });

const GameTestRegistration assetsReplacementPatchProvider(
  "AssetsPatchReadTest", "ReplacementFailureUsesHeldReplacementProvider", [](GameTest& fixture) {
    auto directory = File::temporaryDirectory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(*directory)); });
    auto base = File::relativeTo(*directory, "base");
    auto replacement = File::relativeTo(*directory, "replacement");
    auto shadow = File::relativeTo(*directory, "shadow");
    ASSERT_TRUE(writePatchProvider(base, {
      {"preload.config", "[]"}, {"target.json", R"({"base":true})"}
    }));
    ASSERT_TRUE(writePatchProvider(replacement, {
      {"_metadata", R"({"name":"actual_replacement","friendlyName":"Actual replacement provider","scripts":{"onLoad":["/repair.lua"]}})"},
      {"target.json.patch.lua", "function patch(data) return data end"},
      {"replacement.lua", "function patch(data) error('actual_replacement_failure') end"},
      {"repair.lua", R"(
        local original = assets.origin("/target.json.patch.lua")
        assert(assets.patchReplace("/target.json", "/target.json.patch.lua", original, "/replacement.lua"))
      )"}
    }));
    ASSERT_TRUE(writePatchProvider(shadow, {
      {"_metadata", R"({"name":"unexecuted_shadow","friendlyName":"Unexecuted replacement shadow"})"},
      {"target.json.patch.lua", "function patch(data) data.shadow = true return data end"},
      {"replacement.lua", "function patch(data) return data end"}
    }));
    Logger logger;
    Assets::Settings settings{};
    settings.workerPoolSize = 1;
    auto assets = Assets::create(logger, fixture.root().random(), fixture.root().logMap(), settings,
        {base, replacement, shadow});
    ASSERT_TRUE(assets) << assets.error().what();
    auto origin = (*assets)->assetDescriptor("/replacement.lua");
    ASSERT_TRUE(origin);
    ASSERT_TRUE(*origin);
    EXPECT_EQ((*assets)->assetSourcePath((*origin)->source), std::optional<String>(shadow));
    auto loaded = (*assets)->json("/target.json");
    ASSERT_FALSE(loaded);
    auto const& summary = loaded.error().diagnosticSummary();
    ASSERT_TRUE(summary);
    EXPECT_EQ(summary->kind, ExceptionSummaryKind::PatchRuntime);
    EXPECT_EQ(summary->asset, "/target.json");
    EXPECT_EQ(summary->patch, "/replacement.lua");
    EXPECT_NE(summary->reason.find("actual_replacement_failure"), std::string::npos);
    EXPECT_NE(summary->patchProvider.find(replacement.utf8()), std::string::npos);
    EXPECT_NE(summary->patchProvider.find("actual_replacement"), std::string::npos);
    EXPECT_EQ(summary->patchProvider.find(shadow.utf8()), std::string::npos);
    std::string_view diagnostic = loaded.error().what();
    EXPECT_NE(diagnostic.find("Actual replacement provider"), std::string_view::npos);
    EXPECT_EQ(diagnostic.find("Unexecuted replacement shadow"), std::string_view::npos);
  });

const GameTestRegistration assetsSourceBytesReadFailure(
  "AssetsPatchReadTest", "SourceBytesReadFailureKeepsRequestedProvider", [](GameTest& fixture) {
    auto directory = File::temporaryDirectory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(*directory)); });
    auto original = File::relativeTo(*directory, "original");
    auto shadow = File::relativeTo(*directory, "shadow");
    ASSERT_TRUE(writePatchProvider(original, {
      {"_metadata", R"({"name":"raw_original","friendlyName":"Raw original provider"})"},
      {"preload.config", "[]"}, {"raw.txt", "original bytes"},
      {"caller.json", "{}"},
      {"caller.json.patch.lua", R"(function patch(data)
        for path, metadata in pairs(assets.sourcePaths(true)) do
          if metadata.name == "raw_original" then
            data.raw = assets.sourceBytes(path, "/raw.txt")
            return data
          end
        end
        error("missing raw provider")
      end)"}
    }));
    ASSERT_TRUE(writePatchProvider(shadow, {
      {"_metadata", R"({"name":"raw_shadow","friendlyName":"Raw shadow provider"})"},
      {"raw.txt", "shadow bytes"}
    }));
    Logger logger;
    Assets::Settings settings{};
    settings.workerPoolSize = 1;
    auto assets = Assets::create(logger, fixture.root().random(), fixture.root().logMap(), settings, {original, shadow});
    ASSERT_TRUE(assets) << assets.error().what();
    ASSERT_TRUE(File::remove(File::relativeTo(original, "raw.txt")));
    auto loaded = (*assets)->json("/caller.json");
    ASSERT_FALSE(loaded);
    auto const& summary = loaded.error().diagnosticSummary();
    ASSERT_TRUE(summary);
    EXPECT_EQ(summary->asset, "/caller.json");
    EXPECT_NE(summary->assetProvider.find(original.utf8()), std::string::npos);
    EXPECT_NE(summary->assetProvider.find("raw_original"), std::string::npos);
    EXPECT_EQ(summary->assetProvider.find(shadow.utf8()), std::string::npos);
    std::string_view diagnostic = loaded.error().what();
    EXPECT_NE(diagnostic.find("Raw original provider"), std::string_view::npos);
    EXPECT_NE(diagnostic.find("/raw.txt"), std::string_view::npos);
    EXPECT_EQ(diagnostic.find("Raw shadow provider"), std::string_view::npos);
  });

const GameTestRegistration assetsRetargetSourceSpecificPatch(
  "AssetsPatchReadTest", "SourceSpecificRetargetAppendsAndPreservesForeignPatches", [](GameTest& fixture) {
    auto directory = File::temporaryDirectory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(*directory)); });
    auto base = File::relativeTo(*directory, "base");
    auto original = File::relativeTo(*directory, "original");
    auto foreign = File::relativeTo(*directory, "foreign");
    auto repair = File::relativeTo(*directory, "repair");
    ASSERT_TRUE(writePatchProvider(base, {
      {"preload.config", "[]"},
      {"old.json", R"({"weights":{"old":1},"trace":[]})"},
      {"new.json", R"({"weights":{"new":1},"trace":[]})"},
      {"old.json.patch", R"([{"op":"add","path":"/trace/-","value":"early-old"}])"},
      {"new.json.patch", R"([{"op":"add","path":"/trace/-","value":"early-new"}])"}
    }));
    ASSERT_TRUE(writePatchProvider(original, {
      {"_metadata", R"({"name":"retarget_original"})"},
      {"disable.patch", R"([{"op":"replace","path":"/weights/old","value":0},{"op":"add","path":"/trace/-","value":"original"}])"},
      {"zz.patchlist", R"([{"paths":["/old.json"],"patches":["/disable.patch","/disable.patch"]}])"}
    }));
    ASSERT_TRUE(writePatchProvider(foreign, {
      {"disable.patch", R"([{"op":"add","path":"/trace/-","value":"foreign"}])"},
      {"zz.patchlist", R"([{"paths":["/old.json","/new.json"],"patches":["/disable.patch"]}])"}
    }));
    ASSERT_TRUE(writePatchProvider(repair, {
      {"_metadata", R"({"scripts":{"postLoad":["/retarget.lua"]}})"},
      {"retarget.lua", R"(
        local original
        for path, metadata in pairs(assets.sourcePaths(true)) do
          if metadata.name == "retarget_original" then original = path end
        end
        assert(original)
        assets.add("/replacement.patch", '[{"op":"replace","path":"/weights/new","value":0},{"op":"add","path":"/trace/-","value":"repaired"}]')
        assert(not assets.patchReplace("/old.json", "/disable.patch", original, "/replacement.patch", "/missing.json"))
        local old, new = assets.json("/old.json"), assets.json("/new.json")
        assert(old.weights.old == 0 and table.concat(old.trace, ",") == "early-old,original,original,foreign")
        assert(new.weights.new == 1 and table.concat(new.trace, ",") == "early-new,foreign")
        assert(assets.patchReplace("/old.json", "/disable.patch", original, "/replacement.patch", "/new.json"))
        old, new = assets.json("/old.json"), assets.json("/new.json")
        assert(old.weights.old == 1 and table.concat(old.trace, ",") == "early-old,foreign")
        assert(new.weights.new == 0 and table.concat(new.trace, ",") == "early-new,foreign,repaired,repaired")
        assets.add("/retarget-observed.json", {old = old, new = new})
      )"}
    }));
    Logger logger;
    Assets::Settings settings{};
    settings.workerPoolSize = 1;
    auto assets = Assets::create(logger, fixture.root().random(), fixture.root().logMap(), settings,
        {base, original, foreign, repair});
    ASSERT_TRUE(assets) << assets.error().what();
    auto expected = Json::parseJson(R"({"old":{"weights":{"old":1},"trace":["early-old","foreign"]},"new":{"weights":{"new":0},"trace":["early-new","foreign","repaired","repaired"]}})");
    ASSERT_TRUE(expected);
    auto observed = (*assets)->json("/retarget-observed.json");
    ASSERT_TRUE(observed) << observed.error().what();
    EXPECT_EQ(*observed, *expected);
    auto old = (*assets)->json("/old.json");
    auto replacement = (*assets)->json("/new.json");
    ASSERT_TRUE(old);
    ASSERT_TRUE(replacement);
    auto expectedOld = expected->get("old");
    auto expectedNew = expected->get("new");
    ASSERT_TRUE(expectedOld);
    ASSERT_TRUE(expectedNew);
    EXPECT_EQ(*old, *expectedOld);
    EXPECT_EQ(*replacement, *expectedNew);
  });

const GameTestRegistration assetsLoadScriptReplacement(
  "AssetsPatchReadTest", "LoadScriptReplacementPreservesDeclarationsAndProviderIsolation", [](GameTest& fixture) {
    auto directory = File::temporaryDirectory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(*directory)); });
    auto original = File::relativeTo(*directory, "original");
    auto shadow = File::relativeTo(*directory, "shadow");
    auto repair = File::relativeTo(*directory, "repair");
    ASSERT_TRUE(writePatchProvider(original, {
      {"_metadata", R"({"name":"load_original","scripts":{"onLoad":["/already.lua"],"postLoad":["/first.lua","/shared-load.lua","/last.lua"]}})"},
      {"preload.config", "[]"},
      {"already.lua", "assets.add('/already.json', {executed = true})"},
      {"first.lua", "loadOrder = 'before'"},
      {"shared-load.lua", "loadOrder = loadOrder .. ',original'"},
      {"last.lua", "assets.add('/load-observed.json', {trace = loadOrder .. ',after'})"},
      {"ordinary.lua", "error('ordinary game script must remain ordinary')"},
      {"replacement.lua", "error('indexed replacement must not run instead of newFiles')"}
    }));
    ASSERT_TRUE(writePatchProvider(shadow, {
      {"_metadata", R"({"name":"load_shadow","scripts":{"postLoad":["/shared-load.lua"]}})"},
      {"shared-load.lua", "assets.add('/shadow-load.json', {executed = true})"}
    }));
    ASSERT_TRUE(writePatchProvider(repair, {
      {"_metadata", R"({"name":"load_repair","scripts":{"onLoad":["/setup.lua"],"postLoad":["/check.lua"]}})"},
      {"setup.lua", R"(
        local original
        for path, metadata in pairs(assets.sourcePaths(true)) do
          if metadata.name == "load_original" then original = path end
        end
        assert(original)
        assets.add("/replacement.lua", "loadOrder = loadOrder .. ',repaired'")
        local own = assets.origin("/setup.lua")
        assert(not assets.loadScriptReplace(own, "/setup.lua", "/replacement.lua"))
        assert(not assets.loadScriptReplace(original, "/already.lua", "/replacement.lua"))
        assert(not assets.loadScriptReplace("unknown-source", "/shared-load.lua", "/replacement.lua"))
        assert(not assets.loadScriptReplace(original, "/ordinary.lua", "/replacement.lua"))
        assert(not assets.loadScriptReplace(original, "/../shared-load.lua", "/replacement.lua"))
        assert(assets.loadScriptReplace(original, "/shared-load.lua", "/replacement.lua"))
        assert(not assets.loadScriptReplace(original, "/shared-load.lua", "/missing.lua"))
        assets.add("/setup-observed.json", {registered = true})
      )"},
      {"check.lua", R"lua(
        local original
        for path, metadata in pairs(assets.sourcePaths(true)) do
          if metadata.name == "load_original" then original = path end
        end
        assert(not assets.loadScriptReplace(original, "/shared-load.lua", "/replacement.lua"))
        assert(assets.sourceBytes(original, "/shared-load.lua") == "loadOrder = loadOrder .. ',original'")
        assert(assets.bytes("/shared-load.lua") == "assets.add('/shadow-load.json', {executed = true})")
        assets.add("/check-observed.json", {unchanged = true})
      )lua"}
    }));
    Logger logger;
    Assets::Settings settings{};
    settings.workerPoolSize = 1;
    auto assets = Assets::create(logger, fixture.root().random(), fixture.root().logMap(), settings,
        {original, shadow, repair});
    ASSERT_TRUE(assets) << assets.error().what();
    auto observed = (*assets)->json("/load-observed.json");
    ASSERT_TRUE(observed) << observed.error().what();
    EXPECT_EQ(*observed, Json(JsonObject{{"trace", "before,repaired,after"}}));
    for (String const& path : {String("/already.json"), String("/shadow-load.json"),
        String("/setup-observed.json"), String("/check-observed.json")}) {
      auto loaded = (*assets)->json(path);
      ASSERT_TRUE(loaded) << loaded.error().what();
    }
    auto ordinary = (*assets)->bytes("/ordinary.lua");
    ASSERT_TRUE(ordinary);
    EXPECT_EQ(String(std::string_view((*ordinary)->begin(), (*ordinary)->end())),
        "error('ordinary game script must remain ordinary')");
  });

const GameTestRegistration assetsLoadScriptReplacementProvider(
  "AssetsPatchReadTest", "LoadScriptReplacementFailureUsesHeldProviderAndKeepsFailurePolicy", [](GameTest& fixture) {
    for (bool syntax : {false, true}) {
      SCOPED_TRACE(syntax ? "compile" : "invoke");
      auto directory = File::temporaryDirectory(fixture.root().random());
      ASSERT_TRUE(directory);
      auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(*directory)); });
      auto original = File::relativeTo(*directory, "original");
      auto replacement = File::relativeTo(*directory, "replacement");
      auto shadow = File::relativeTo(*directory, "shadow");
      ASSERT_TRUE(writePatchProvider(original, {
        {"_metadata", R"({"name":"load_target","scripts":{"postLoad":["/shared-load.lua","/skipped.lua"]}})"},
        {"preload.config", "[]"},
        {"shared-load.lua", "assets.add('/unexecuted-original.json', {})"},
        {"skipped.lua", "assets.add('/skipped.json', {})"}
      }));
      ASSERT_TRUE(writePatchProvider(replacement, {
        {"_metadata", R"({"name":"actual_load_replacement","friendlyName":"Actual load replacement provider","scripts":{"onLoad":["/setup.lua"],"postLoad":["/check-skipped.lua"]}})"},
        {"replacement.lua", syntax ? String("this is invalid Lua syntax") : String("error('held_load_replacement_failure')")},
        {"setup.lua", R"(
          local original = assets.origin("/shared-load.lua")
          assert(assets.loadScriptReplace(original, "/shared-load.lua", "/replacement.lua"))
          assets.add("/setup-observed.json", {registered = true})
        )"},
        {"check-skipped.lua", R"(
          local original
          for path, metadata in pairs(assets.sourcePaths(true)) do
            if metadata.name == "load_target" then original = path end
          end
          assert(not assets.loadScriptReplace(original, "/skipped.lua", "/replacement.lua"))
          assets.add("/skipped-check.json", {closed = true})
        )"}
      }));
      ASSERT_TRUE(writePatchProvider(shadow, {
        {"_metadata", R"({"name":"unexecuted_load_shadow","friendlyName":"Unexecuted load replacement shadow","scripts":{"postLoad":["/shared-load.lua"]}})"},
        {"shared-load.lua", "assets.add('/shadow-load.json', {executed = true})"},
        {"replacement.lua", "error('wrong_shadow_load_failure')"}
      }));
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
      auto assets = Assets::create(logger, fixture.root().random(), fixture.root().logMap(), settings,
          {original, replacement, shadow});
      ASSERT_TRUE(assets) << assets.error().what();
      EXPECT_NE(errors->messages.find("/replacement.lua"), std::string::npos);
      EXPECT_NE(errors->messages.find("Actual load replacement provider"), std::string::npos);
      EXPECT_NE(errors->messages.find(replacement.utf8()), std::string::npos);
      EXPECT_EQ(errors->messages.find("Unexecuted load replacement shadow"), std::string::npos);
      EXPECT_EQ(errors->messages.find("wrong_shadow_load_failure"), std::string::npos);
      if (!syntax)
        EXPECT_NE(errors->messages.find("held_load_replacement_failure"), std::string::npos);
      for (String const& path : {String("/unexecuted-original.json"), String("/skipped.json")}) {
        auto exists = (*assets)->assetExists(path);
        ASSERT_TRUE(exists);
        EXPECT_FALSE(*exists);
      }
      for (String const& path : {String("/setup-observed.json"), String("/skipped-check.json"), String("/shadow-load.json")}) {
        auto loaded = (*assets)->json(path);
        ASSERT_TRUE(loaded) << loaded.error().what();
      }
    }
  });

const GameTestRegistration assetsPackedSourceBytes(
  "AssetsPatchReadTest", "SourceBytesPreservesExactPackedMembershipAndRawBinary", [](GameTest& fixture) {
    auto directory = File::temporaryDirectory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(*directory)); });
    auto original = File::relativeTo(*directory, "original");
    auto shadow = File::relativeTo(*directory, "shadow");
    auto packed = File::relativeTo(*directory, "original.pak");
    ASSERT_TRUE(writePatchProvider(original, {
      {"_metadata", R"({"name":"packed_original"})"}, {"preload.config", "[]"},
      {"CaseSensitive.txt", String(std::string_view("one\0two", 7))}
    }));
    ASSERT_TRUE(writePatchProvider(shadow, {
      {"_metadata", R"({"scripts":{"postLoad":["/read-packed.lua"]}})"},
      {"CaseSensitive.txt", "shadow"},
      {"read-packed.lua", R"(
        local original
        for path, metadata in pairs(assets.sourcePaths(true)) do
          if metadata.name == "packed_original" then original = path end
        end
        assert(original)
        local raw = assets.sourceBytes(original, "/CaseSensitive.txt")
        assert(raw == "one\0two" and #raw == 7)
        assert(assets.bytes("/CaseSensitive.txt") == "shadow")
        for _, path in ipairs({"/casesensitive.txt", "/_metadata", "/../original/CaseSensitive.txt"}) do
          local ok, message = pcall(assets.sourceBytes, original, path)
          assert(not ok and message:find(original, 1, true) and message:find(path, 1, true))
        end
        assets.add("/packed-observed.json", {length = #raw, exact = true})
      )"}
    }));
    Logger logger;
    auto source = DirectoryAssetSource::create(logger, original);
    ASSERT_TRUE(source) << source.error().what();
    auto built = PackedAssetSource::build(**source, packed, {}, {});
    ASSERT_TRUE(built) << built.error().what();
    Assets::Settings settings{};
    settings.workerPoolSize = 1;
    auto assets = Assets::create(logger, fixture.root().random(), fixture.root().logMap(), settings, {packed, shadow});
    ASSERT_TRUE(assets) << assets.error().what();
    auto observed = (*assets)->json("/packed-observed.json");
    ASSERT_TRUE(observed) << observed.error().what();
    EXPECT_EQ(*observed, Json(JsonObject{{"length", 7}, {"exact", true}}));
  });
