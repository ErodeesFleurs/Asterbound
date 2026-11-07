import std;
import star.algorithm;
import star.assets;
import star.asset_path;
import star.buffer;
import star.byte_array;
import star.file;
import star.format;
import star.game_test;
import star.image;
import star.io_device;
import star.list;
import star.logging;
import star.root;
import star.string;
import star.vector;

#include "gtest/gtest.h"

using namespace Star;

namespace {
Assets::Settings referenceCacheSettings(std::uint64_t budget = 8, float referenceTtl = 300) {
  Assets::Settings settings{};
  settings.assetTimeToLive = -1;
  settings.workerPoolSize = 0;
  settings.referenceImageTimeToLive = referenceTtl;
  settings.referenceImagePixelBudget = budget;
  return settings;
}

std::expected<void, StarException> writeReferenceCacheAssets(GameTest& fixture, String const& directory) {
  auto preload = File::write_file(String("[]"), File::relative_to(directory, "preload.config"));
  if (!preload)
    return std::unexpected(std::move(preload.error()));
  struct PngFixture {
    std::string_view name;
    Vec2U size;
    Vec4B color;
  };
  for (auto const& png : std::array{
         PngFixture{"base.png", {2, 2}, {255, 255, 255, 255}}, PngFixture{"ordinary.png", {1, 1}, {0, 0, 255, 255}},
         PngFixture{"red.png", {2, 2}, {255, 0, 0, 255}}, PngFixture{"green.png", {2, 2}, {0, 255, 0, 255}},
         PngFixture{"mask.png", {2, 2}, {0, 0, 0, 128}}, PngFixture{"large.png", {3, 2}, {0, 0, 255, 255}}}) {
    auto buffer = std::make_shared<Buffer>();
    auto written = Image::filled(png.size, png.color).writePng(fixture.root().logger(), buffer);
    if (!written)
      return std::unexpected(std::move(written.error()));
    auto stored = File::write_file(buffer->takeData(), File::relative_to(directory, png.name));
    if (!stored)
      return std::unexpected(std::move(stored.error()));
  }
  return {};
}

std::expected<std::shared_ptr<Assets>, StarException> referenceCacheAssets(GameTest& fixture, String const& directory,
                                                                           Assets::Settings const& settings) {
  return Assets::create(fixture.root().logger(), fixture.root().random(), fixture.root().logMap(), settings,
                        {directory});
}

// No decoded strong handle escapes this helper, including the expected result.
void observeImage(Assets& assets, String const& path, Vec2U size, Vec4B color, std::weak_ptr<Image const>& lifetime) {
  SCOPED_TRACE(path.utf8());
  auto image = assets.image(path);
  ASSERT_TRUE(image) << image.error().what();
  ASSERT_TRUE(*image);
  lifetime = *image;
  ASSERT_EQ((*image)->size(), size);
  for (unsigned y = 0; y < size[1]; ++y) {
    for (unsigned x = 0; x < size[0]; ++x) {
      auto pixel = (*image)->get(x, y);
      ASSERT_TRUE(pixel) << pixel.error().what();
      EXPECT_EQ(*pixel, color);
    }
  }
}

void expectIndependentImage(std::weak_ptr<Image const> const& result, std::weak_ptr<Image const> const& source) {
  ASSERT_FALSE(result.expired());
  ASSERT_FALSE(source.expired());
  EXPECT_NE(result.lock().get(), source.lock().get());
}
}// namespace

const GameTestRegistration
  assetsReferenceCacheRetainsBlendAndMask("AssetsTest", "ReferenceCacheRetainsBlendAndMask", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    ASSERT_TRUE(writeReferenceCacheAssets(fixture, *directory));
    auto assets = referenceCacheAssets(fixture, *directory, referenceCacheSettings());
    ASSERT_TRUE(assets) << assets.error().what();
    std::weak_ptr<Image const> ordinary, base, blendReference, maskReference, blend, mask;
    observeImage(**assets, "/ordinary.png", {1, 1}, {0, 0, 255, 255}, ordinary);
    observeImage(**assets, "/base.png", {2, 2}, {255, 255, 255, 255}, base);
    observeImage(**assets, "/red.png", {2, 2}, {255, 0, 0, 255}, blendReference);
    observeImage(**assets, "/mask.png", {2, 2}, {0, 0, 0, 128}, maskReference);
    observeImage(**assets, "/base.png?blendmult=/red.png", {2, 2}, {255, 0, 0, 255}, blend);
    observeImage(**assets, "/base.png?addmask=/mask.png", {2, 2}, {255, 255, 255, 128}, mask);
    expectIndependentImage(blend, blendReference);
    expectIndependentImage(blend, base);
    expectIndependentImage(mask, maskReference);
    expectIndependentImage(mask, base);

    (*assets)->cleanup();
    EXPECT_TRUE(ordinary.expired());
    EXPECT_TRUE(base.expired());
    EXPECT_TRUE(blend.expired());
    EXPECT_TRUE(mask.expired());
    EXPECT_FALSE(blendReference.expired());
    EXPECT_FALSE(maskReference.expired());
  });

const GameTestRegistration assetsReferenceCacheExtensionCanBeDisabled(
  "AssetsTest", "ReferenceCacheExtensionCanBeDisabled", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    ASSERT_TRUE(writeReferenceCacheAssets(fixture, *directory));
    for (auto const& settings :
         std::array{referenceCacheSettings(0), referenceCacheSettings(4, -1), referenceCacheSettings(4, -2)}) {
      SCOPED_TRACE(::testing::Message() << "budget=" << settings.referenceImagePixelBudget
                                        << " reference TTL=" << settings.referenceImageTimeToLive);
      auto assets = referenceCacheAssets(fixture, *directory, settings);
      ASSERT_TRUE(assets) << assets.error().what();
      std::weak_ptr<Image const> reference, result;
      observeImage(**assets, "/red.png", {2, 2}, {255, 0, 0, 255}, reference);
      observeImage(**assets, "/base.png?blendmult=/red.png", {2, 2}, {255, 0, 0, 255}, result);
      expectIndependentImage(result, reference);
      (*assets)->cleanup();
      EXPECT_TRUE(result.expired());
      EXPECT_TRUE(reference.expired());
    }
  });

const GameTestRegistration assetsReferenceCacheSkipsOversizedCandidates(
  "AssetsTest", "ReferenceCacheSkipsOversizedCandidates", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    ASSERT_TRUE(writeReferenceCacheAssets(fixture, *directory));
    auto assets = referenceCacheAssets(fixture, *directory, referenceCacheSettings(4));
    ASSERT_TRUE(assets) << assets.error().what();
    std::weak_ptr<Image const> small, large, smallResult, largeResult;
    observeImage(**assets, "/red.png", {2, 2}, {255, 0, 0, 255}, small);
    observeImage(**assets, "/large.png", {3, 2}, {0, 0, 255, 255}, large);
    observeImage(**assets, "/base.png?blendmult=/red.png", {2, 2}, {255, 0, 0, 255}, smallResult);
    // The newest candidate cannot fit, but must not prevent the older one fitting exactly.
    observeImage(**assets, "/base.png?blendmult=/large.png", {2, 2}, {0, 0, 255, 255}, largeResult);
    expectIndependentImage(smallResult, small);
    expectIndependentImage(largeResult, large);
    (*assets)->cleanup();
    EXPECT_FALSE(small.expired());
    EXPECT_TRUE(large.expired());
    EXPECT_TRUE(smallResult.expired());
    EXPECT_TRUE(largeResult.expired());
  });

const GameTestRegistration assetsReferenceCacheRecentReferenceHitWinsBudget(
  "AssetsTest", "ReferenceCacheRecentReferenceHitWinsBudget", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    ASSERT_TRUE(writeReferenceCacheAssets(fixture, *directory));
    auto assets = referenceCacheAssets(fixture, *directory, referenceCacheSettings(4));
    ASSERT_TRUE(assets) << assets.error().what();
    std::weak_ptr<Image const> red, green, first, second, recent;
    observeImage(**assets, "/red.png", {2, 2}, {255, 0, 0, 255}, red);
    observeImage(**assets, "/green.png", {2, 2}, {0, 255, 0, 255}, green);
    observeImage(**assets, "/base.png?blendmult=/red.png", {2, 2}, {255, 0, 0, 255}, first);
    observeImage(**assets, "/base.png?blendmult=/green.png", {2, 2}, {0, 255, 0, 255}, second);
    // A new generated path must resolve red again, hitting its cached reference wrapper.
    observeImage(**assets, "/base.png?flipx?blendmult=/red.png", {2, 2}, {255, 0, 0, 255}, recent);
    expectIndependentImage(first, red);
    expectIndependentImage(second, green);
    expectIndependentImage(recent, red);
    expectIndependentImage(recent, first);
    (*assets)->cleanup();
    EXPECT_FALSE(red.expired());
    EXPECT_TRUE(green.expired());
    EXPECT_TRUE(first.expired());
    EXPECT_TRUE(second.expired());
    EXPECT_TRUE(recent.expired());
  });

const GameTestRegistration assetsReferenceCacheClearDisregardsExtendedTtl(
  "AssetsTest", "ReferenceCacheClearDisregardsExtendedTtl", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    ASSERT_TRUE(writeReferenceCacheAssets(fixture, *directory));
    auto assets = referenceCacheAssets(fixture, *directory, referenceCacheSettings(4));
    ASSERT_TRUE(assets) << assets.error().what();
    std::weak_ptr<Image const> reference, result;
    observeImage(**assets, "/red.png", {2, 2}, {255, 0, 0, 255}, reference);
    observeImage(**assets, "/base.png?blendmult=/red.png", {2, 2}, {255, 0, 0, 255}, result);
    expectIndependentImage(result, reference);
    (*assets)->cleanup();
    EXPECT_TRUE(result.expired());
    ASSERT_FALSE(reference.expired());
    (*assets)->clearCache();
    EXPECT_TRUE(reference.expired());
  });

const GameTestRegistration assetsReferenceCacheExternalHandlesPersist(
  "AssetsTest", "ReferenceCacheExternalHandlesPersist", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    ASSERT_TRUE(writeReferenceCacheAssets(fixture, *directory));
    for (std::uint64_t budget : {0u, 4u}) {
      SCOPED_TRACE(::testing::Message() << "budget=" << budget);
      auto assets = referenceCacheAssets(fixture, *directory, referenceCacheSettings(budget));
      ASSERT_TRUE(assets) << assets.error().what();
      auto ordinary = (*assets)->image("/ordinary.png");
      auto reference = (*assets)->image("/large.png");
      ASSERT_TRUE(ordinary) << ordinary.error().what();
      ASSERT_TRUE(reference) << reference.error().what();
      std::weak_ptr<Image const> ordinaryLifetime = *ordinary;
      std::weak_ptr<Image const> referenceLifetime = *reference;
      std::weak_ptr<Image const> result;
      observeImage(**assets, "/base.png?blendmult=/large.png", {2, 2}, {0, 0, 255, 255}, result);
      expectIndependentImage(result, referenceLifetime);
      (*assets)->cleanup();
      EXPECT_TRUE(result.expired());
      EXPECT_FALSE(ordinaryLifetime.expired());
      EXPECT_FALSE(referenceLifetime.expired());
      (*assets)->clearCache();
      auto ordinaryAgain = (*assets)->image("/ordinary.png");
      auto referenceAgain = (*assets)->image("/large.png");
      ASSERT_TRUE(ordinaryAgain) << ordinaryAgain.error().what();
      ASSERT_TRUE(referenceAgain) << referenceAgain.error().what();
      EXPECT_EQ(ordinaryAgain->get(), ordinary->get());
      EXPECT_EQ(referenceAgain->get(), reference->get());
      EXPECT_EQ((*ordinary)->get32(0, 0), Vec4B(0, 0, 255, 255));
      EXPECT_EQ((*reference)->get32(2, 1), Vec4B(0, 0, 255, 255));
      ordinaryAgain->reset();
      referenceAgain->reset();
      ordinary->reset();
      reference->reset();
      (*assets)->cleanup();
      EXPECT_TRUE(ordinaryLifetime.expired());
      EXPECT_TRUE(referenceLifetime.expired());
    }
  });
