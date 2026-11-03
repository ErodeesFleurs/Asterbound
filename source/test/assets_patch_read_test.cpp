import std;
import star.algorithm;
import star.assets;
import star.buffer;
import star.byte_array;
import star.file;
import star.format;
import star.game_test;
import star.image;
import star.json;
import star.list;
import star.logging;
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
