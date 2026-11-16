import std;
import star.algorithm;
import star.asset_path;
import star.assets;
import star.audio;
import star.buffer;
import star.byte_array;
import star.data_stream_devices;
import star.data_stream_extra;
import star.file;
import star.font;
import star.format;
import star.game_test;
import star.image;
import star.image_lua_bindings;
import star.json;
import star.list;
import star.logging;
import star.lua;
import star.lua_converters;
import star.lua_root;
import star.path_embed;
import star.random;
import star.root;
import star.root_lua_bindings;
import star.string;
import star.utility_lua_bindings;
import star.vector;

#include "gtest/gtest.h"

using namespace Star;

namespace {

// Every negative load belongs to this logger, never the game fixture's error sink.
struct EmbeddedFixture {
  Logger logger;
  RandomService random{0x454d424544444544};
  LogMap logMap;
  String directory;
  std::shared_ptr<Assets> assets;
  LuaEnginePtr engine;
  std::optional<LuaContext> lua;

  ~EmbeddedFixture() {
    lua.reset();
    engine.reset();
    assets.reset();
    if (!directory.empty())
      EXPECT_TRUE(File::remove_all(directory));
  }

  std::expected<void, StarException> init(GameTest& fixture, bool allowRemote = true, bool installedSources = false) {
    logger.remove_stdout_sink();
    auto temporary = File::temporary_directory(random);
    if (!temporary)
      return std::unexpected(std::move(temporary.error()));
    directory = std::move(*temporary);
    auto written = File::write_file(String("[]"), File::relative_to(directory, "preload.config"));
    if (!written)
      return std::unexpected(std::move(written.error()));
    written = File::write_file(String(R"({"frameGrid":{"size":[1,2],"dimensions":[2,1],"begin":[1,0],"names":[["left","right"]]},"aliases":{"alias":"left"}})"),
                               File::relative_to(directory, "embedded.frames"));
    if (!written)
      return std::unexpected(std::move(written.error()));
    Assets::Settings settings{};
    settings.workerPoolSize = 1;
    settings.assetTimeToLive = std::numeric_limits<float>::max();
    settings.missingImage = {};
    settings.missingAudio = {};
    settings.allowRemoteAssets = allowRemote;
    auto sources = installedSources ? fixture.rootSettings().assetSources : StringList{};
    sources.push_back(directory);
    auto createdAssets = Assets::create(logger, random, logMap, settings, std::move(sources));
    if (!createdAssets)
      return std::unexpected(std::move(createdAssets.error()));
    assets = std::move(*createdAssets);
    auto createdEngine = LuaEngine::create();
    if (!createdEngine)
      return std::unexpected(std::move(createdEngine.error()));
    engine = std::move(*createdEngine);
    auto context = engine->createContext();
    if (!context)
      return std::unexpected(std::move(context.error()));
    lua.emplace(std::move(*context));
    auto utilities = LuaBindings::makeUtilityCallbacks(random, logger, logMap);
    if (!utilities)
      return std::unexpected(std::move(utilities.error()));
    return lua->setCallbacks("sb", *utilities);
  }
};

std::expected<Image, StarException> embeddedPixels() {
  Image image(4, 2, PixelFormat::RGBA32);
  for (unsigned y = 0; y != 2; ++y) {
    for (unsigned x = 0; x != 4; ++x) {
      auto set = image.set(x, y, Vec4B{static_cast<std::uint8_t>(23 + x * 51),
                                      static_cast<std::uint8_t>(37 + y * 97),
                                      static_cast<std::uint8_t>(x + y * 4),
                                      static_cast<std::uint8_t>(71 + x * 17 + y * 53)});
      if (!set)
        return std::unexpected(std::move(set.error()));
    }
  }
  return image;
}

void expectPixels(Image const& actual, Image const& expected) {
  ASSERT_EQ(actual.size(), expected.size());
  for (unsigned y = 0; y != expected.height(); ++y) {
    for (unsigned x = 0; x != expected.width(); ++x) {
      auto actualPixel = actual.get(x, y);
      auto expectedPixel = expected.get(x, y);
      ASSERT_TRUE(actualPixel);
      ASSERT_TRUE(expectedPixel);
      EXPECT_EQ(*actualPixel, *expectedPixel) << x << ',' << y;
    }
  }
}

std::expected<String, StarException> binaryPath(std::span<char const> bytes,
    EmbeddedCompressionMethod compression = EmbeddedCompressionMethod::Zstd) {
  auto payload = embedAndCompressData(bytes, compression);
  if (!payload)
    return std::unexpected(std::move(payload.error()));
  return "/opensb_data;bin;" + *payload;
}

std::expected<Root::Settings, StarException> embeddedRootSettings(GameTest& fixture, String const& directory,
                                                                 bool allowRemote) {
  auto settings = fixture.rootSettings();
  settings.storageDirectory = File::relative_to(directory, "storage");
  settings.runtimeConfigFile = {};
  settings.logDirectory = {};
  settings.logFile = {};
  settings.quiet = true;
  settings.includeUGC = false;
  settings.assetsSettings.missingImage = {};
  settings.assetsSettings.missingAudio = {};
  settings.assetsSettings.assetTimeToLive = std::numeric_limits<float>::max();
  settings.assetSources.push_back(directory);
  auto config = settings.defaultConfiguration.setPath("safe.allowRemoteAssets", allowRemote);
  if (!config)
    return std::unexpected(std::move(config.error()));
  settings.defaultConfiguration = std::move(*config);
  return settings;
}

std::expected<std::shared_ptr<LuaRoot>, StarException> embeddedRootLua(Root& root) {
  auto lua = LuaRoot::create(root);
  if (!lua)
    return std::unexpected(std::move(lua.error()));
  auto callbacks = LuaBindings::makeRootCallbacks(root);
  if (!callbacks)
    return std::unexpected(std::move(callbacks.error()));
  (*lua)->addCallbacks("root", *callbacks);
  auto utilities = LuaBindings::makeUtilityCallbacks(root.random(), root.logger(), root.logMap());
  if (!utilities)
    return std::unexpected(std::move(utilities.error()));
  (*lua)->addCallbacks("sb", *utilities);
  return std::move(*lua);
}

} // namespace

const GameTestRegistration embeddedEncoders("EmbeddedAssetsTest", "LuaEncodersPreserveBytesTextAndJson", [](GameTest& fixture) {
  EmbeddedFixture setup;
  auto initialized = setup.init(fixture);
  ASSERT_TRUE(initialized) << initialized.error().what();
  auto& lua = *setup.lua;
  std::string arbitrary;
  for (unsigned byte = 0; byte != 256; ++byte)
    arbitrary.push_back(static_cast<char>(byte));
  arbitrary.append("\0tail", 5);
  ASSERT_TRUE(lua.setPath("binary", String(arbitrary)));
  String text(std::string("%:;?/\0", 6) + "雪 /opensb_data;text;literal");
  ASSERT_TRUE(lua.setPath("text", text));
  Json value(JsonObject{{"nested", JsonObject{{"values", JsonArray{false, 19, "雪%:;?/"}}}},
                        {"nul", String(std::string("a\0b", 3))}});
  ASSERT_TRUE(lua.setPath("value", value));
  for (auto const& compression : {String("nOnE"), String("ZlIb"), String("zStD")}) {
    SCOPED_TRACE(compression.utf8());
    ASSERT_TRUE(lua.setPath("compression", compression));
    auto path = lua.eval<String>("return sb.embedData(binary, compression)");
    ASSERT_TRUE(path) << path.error().what();
    EXPECT_TRUE(AssetPath::isEmbedded(*path));
    EXPECT_TRUE(AssetPath::isRemote(*path));
    EXPECT_FALSE(setup.assets->bytes(*path));
    auto bytes = setup.assets->bytes(*path, true);
    ASSERT_TRUE(bytes) << bytes.error().what();
    EXPECT_EQ(**bytes, ByteArray::from_string(arbitrary));
    // A trusted raw read must not turn the cache into a permission bypass.
    auto denied = setup.assets->bytes(*path);
    ASSERT_FALSE(denied);
    EXPECT_TRUE(denied.error().is<AssetException>());
    auto jsonPath = lua.eval<String>("return sb.embedJson(value, compression)");
    ASSERT_TRUE(jsonPath) << jsonPath.error().what();
    EXPECT_EQ(setup.assets->json(*jsonPath), value);
    EXPECT_EQ(setup.assets->json(*jsonPath + ":nested.values[2]"), Json("雪%:;?/"));
    auto wire = setup.assets->bytes(*jsonPath, true);
    ASSERT_TRUE(wire) << wire.error().what();
    auto decoded = DataStreamBuffer::deserialize<Json>((*wire)->span());
    ASSERT_TRUE(decoded) << decoded.error().what();
    EXPECT_EQ(*decoded, value);
    auto binaryJson = lua.eval<String>(R"LUA(
      return sb.embedData('{"binary":{"answer":43}}', compression)
    )LUA");
    ASSERT_TRUE(binaryJson) << binaryJson.error().what();
    EXPECT_EQ(setup.assets->json(*binaryJson + ":binary.answer"), Json(43));
  }
  auto textPath = lua.eval<String>("return sb.embedText(text)");
  ASSERT_TRUE(textPath) << textPath.error().what();
  auto textBytes = setup.assets->bytes(*textPath, true);
  ASSERT_TRUE(textBytes) << textBytes.error().what();
  EXPECT_EQ(**textBytes, ByteArray::from_string(text.utf8()));
  for (String const& source : {String("return sb.embedData(binary, 'gzip')"),
                               String("return sb.embedJson(value, 'unknown')")}) {
    auto rejected = lua.eval<String>(source);
    ASSERT_FALSE(rejected);
    EXPECT_TRUE(rejected.error().is<LuaException>());
    EXPECT_FALSE(std::string_view(rejected.error().what()).empty());
  }
  auto recovered = lua.eval<String>("return sb.embedData(binary, 'None')");
  ASSERT_TRUE(recovered) << recovered.error().what();
  auto recoveredBytes = setup.assets->bytes(*recovered, true);
  ASSERT_TRUE(recoveredBytes);
  EXPECT_EQ(**recoveredBytes, ByteArray::from_string(arbitrary));
  EXPECT_FALSE(AssetPath::isEmbedded("/OPENSB_DATA;text;x"));
});

const GameTestRegistration embeddedImages("EmbeddedAssetsTest", "ImageEmbeddingFramesAliasesOffsetsAndDirectives", [](GameTest& fixture) {
  EmbeddedFixture setup;
  auto initialized = setup.init(fixture);
  ASSERT_TRUE(initialized) << initialized.error().what();
  auto image = embeddedPixels();
  ASSERT_TRUE(image);
  auto& lua = *setup.lua;
  ASSERT_TRUE(lua.setPath("image", *image));
  auto plain = lua.eval<String>("return image:embed('None')");
  ASSERT_TRUE(plain) << plain.error().what();
  auto loaded = setup.assets->image(*plain);
  ASSERT_TRUE(loaded) << loaded.error().what();
  expectPixels(**loaded, *image);
  auto noFrames = setup.assets->imageFrames(*plain);
  ASSERT_TRUE(noFrames) << noFrames.error().what();
  EXPECT_FALSE(*noFrames);
  auto png = setup.assets->bytes(*plain, true);
  ASSERT_TRUE(png);
  auto decoded = Image::read_png(setup.logger, std::make_shared<Buffer>(**png));
  ASSERT_TRUE(decoded) << decoded.error().what();
  expectPixels(*decoded, *image);

  ASSERT_TRUE(lua.load(R"LUA(
    frames = {frameGrid = {size = {1, 2}, dimensions = {2, 1}, begin = {1, 0},
      names = {{"left", "right"}}}, aliases = {alias = "left"}}
  )LUA"));
  for (String const& expression : {
         String("return image:embed('None', frames, 'text')"),
         String("return image:embed('Zlib', frames, 'Zstd')"),
         String("return image:embed('Zstd', frames, 'None')"),
         String("return image:embed('Zstd', frames)"),
         String("return image:embed('None', frames, 'Zlib')"),
         String("return image:embed('Zstd', '/embedded.frames')"),
         String("return image:embed('Zlib', sb.embedJson(frames, 'Zstd'))")}) {
    SCOPED_TRACE(expression.utf8());
    auto path = lua.eval<String>(expression);
    ASSERT_TRUE(path) << path.error().what();
    auto whole = setup.assets->image(*path);
    ASSERT_TRUE(whole) << whole.error().what();
    expectPixels(**whole, *image);
    auto specification = setup.assets->imageFrames(*path);
    ASSERT_TRUE(specification) << specification.error().what();
    ASSERT_TRUE(*specification);
    auto aliasRect = (*specification)->getRect("alias");
    auto leftRect = (*specification)->getRect("left");
    ASSERT_TRUE(aliasRect);
    ASSERT_TRUE(leftRect);
    ASSERT_TRUE(*leftRect);
    EXPECT_EQ(*aliasRect, *leftRect);
    auto alias = setup.assets->image(*path + ":alias");
    ASSERT_TRUE(alias) << alias.error().what();
    auto expected = image->sub_image({1, 0}, {1, 2});
    ASSERT_TRUE(expected);
    expectPixels(**alias, *expected);
    auto right = setup.assets->image(*path + ":right?flipy");
    ASSERT_TRUE(right) << right.error().what();
    ASSERT_EQ((*right)->size(), Vec2U(1, 2));
    for (unsigned y = 0; y != 2; ++y) {
      auto pixel = (*right)->get(0, y);
      auto expectedPixel = image->get(2, 1 - y);
      ASSERT_TRUE(pixel);
      ASSERT_TRUE(expectedPixel);
      EXPECT_EQ(*pixel, *expectedPixel);
    }
    auto missing = setup.assets->image(*path + ":missing");
    ASSERT_FALSE(missing);
    EXPECT_TRUE(missing.error().is<AssetException>());
    EXPECT_NE(std::string_view(missing.error().what()).find("missing"), std::string_view::npos);
    auto again = setup.assets->image(*path + ":alias");
    ASSERT_TRUE(again) << again.error().what();
    expectPixels(**again, *expected);
  }
});

const GameTestRegistration embeddedAudioFont("EmbeddedAssetsTest", "EmbeddedWavAndInstalledFontHaveUsableDecodedContent", [](GameTest& fixture) {
  EmbeddedFixture setup;
  auto initialized = setup.init(fixture, true, true);
  ASSERT_TRUE(initialized) << initialized.error().what();
  constexpr std::array<unsigned char, 48> wav = {
    'R', 'I', 'F', 'F', 40, 0, 0, 0, 'W', 'A', 'V', 'E',
    'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 1, 0,
    0x40, 0x1f, 0, 0, 0x80, 0x3e, 0, 0, 2, 0, 16, 0,
    'd', 'a', 't', 'a', 4, 0, 0, 0, 0x34, 0x12, 0xfe, 0xff};
  ASSERT_TRUE(setup.lua->setPath("wav", String(std::string_view(reinterpret_cast<char const*>(wav.data()), wav.size()))));
  auto audioPath = setup.lua->eval<String>("return sb.embedData(wav, 'Zlib')");
  ASSERT_TRUE(audioPath) << audioPath.error().what();
  auto audio = setup.assets->audio(*audioPath);
  ASSERT_TRUE(audio) << audio.error().what();
  EXPECT_EQ((*audio)->channels(), 1u);
  EXPECT_EQ((*audio)->sampleRate(), 8000u);
  EXPECT_EQ((*audio)->totalSamples(), 2u);
  EXPECT_EQ((*audio)->totalTime(), 2.0 / 8000.0);
  auto decoder = (*audio)->clone();
  ASSERT_TRUE(decoder) << decoder.error().what();
  std::array<std::int16_t, 2> samples{};
  EXPECT_EQ(decoder->read(samples), 2u);
  EXPECT_EQ(samples, (std::array<std::int16_t, 2>{0x1234, -2}));

  auto fonts = setup.assets->scanExtension("ttf");
  ASSERT_TRUE(fonts) << fonts.error().what();
  ASSERT_FALSE(fonts->get().empty()) << "Installed legal assets must supply a TrueType font";
  // Sorting avoids depending on hash iteration order or a mod's override order.
  StringList paths;
  for (auto const& path : fonts->get())
    paths.push_back(path.string());
  paths.sort();
  auto original = setup.assets->font(paths.front());
  ASSERT_TRUE(original) << original.error().what();
  auto originalFace = (*original)->clone();
  ASSERT_TRUE(originalFace);
  auto bytes = setup.assets->bytes(paths.front());
  ASSERT_TRUE(bytes) << bytes.error().what();
  auto fontPath = binaryPath((*bytes)->span());
  ASSERT_TRUE(fontPath);
  auto font = setup.assets->font(*fontPath);
  ASSERT_TRUE(font) << font.error().what();
  auto face = (*font)->clone();
  ASSERT_TRUE(face) << face.error().what();
  ASSERT_TRUE((*face)->setPixelSize(18));
  ASSERT_TRUE((*originalFace)->setPixelSize(18));
  auto exists = (*face)->exists('A');
  ASSERT_TRUE(exists);
  EXPECT_TRUE(*exists);
  auto width = (*face)->width('A');
  ASSERT_TRUE(width);
  EXPECT_GT(*width, 0u);
  auto originalWidth = (*originalFace)->width('A');
  ASSERT_TRUE(originalWidth);
  EXPECT_EQ(*width, *originalWidth);
  auto rendered = (*face)->render('A');
  auto expected = (*originalFace)->render('A');
  ASSERT_TRUE(rendered) << rendered.error().what();
  ASSERT_TRUE(expected) << expected.error().what();
  EXPECT_GT(std::get<0>(*rendered).width(), 0u);
  EXPECT_GT(std::get<0>(*rendered).height(), 0u);
  EXPECT_EQ(std::get<1>(*rendered), std::get<1>(*expected));
  expectPixels(std::get<0>(*rendered), std::get<0>(*expected));
});

const GameTestRegistration embeddedErrors("EmbeddedAssetsTest", "TypedFailuresDoNotPoisonOtherConsumerCaches", [](GameTest& fixture) {
  EmbeddedFixture setup;
  auto initialized = setup.init(fixture);
  ASSERT_TRUE(initialized) << initialized.error().what();
  auto path = setup.lua->eval<String>(R"LUA(return sb.embedText('{"valid":41}'))LUA");
  ASSERT_TRUE(path);
  for (unsigned attempt = 0; attempt != 2; ++attempt) {
    SCOPED_TRACE(attempt);
    auto image = setup.assets->image(*path);
    ASSERT_FALSE(image);
    EXPECT_TRUE(image.error().is<AssetException>());
    EXPECT_NE(std::string_view(image.error().what()).find("PNG binary"), std::string_view::npos);
    auto audio = setup.assets->audio(*path);
    ASSERT_FALSE(audio);
    EXPECT_TRUE(audio.error().is<AssetException>());
    EXPECT_NE(std::string_view(audio.error().what()).find("binary"), std::string_view::npos);
    auto font = setup.assets->font(*path);
    ASSERT_FALSE(font);
    EXPECT_TRUE(font.error().is<AssetException>());
    EXPECT_NE(std::string_view(font.error().what()).find("binary"), std::string_view::npos);
    EXPECT_EQ(setup.assets->json(*path + ":valid"), Json(41));
    auto bytes = setup.assets->bytes(*path, true);
    ASSERT_TRUE(bytes);
    EXPECT_EQ(**bytes, ByteArray::from_string(R"({"valid":41})"));
    EXPECT_FALSE(setup.assets->bytes(*path));
  }
  auto malformed = setup.assets->json("/opensb_data;text;%");
  ASSERT_FALSE(malformed);
  EXPECT_TRUE(malformed.error().is<AssetException>());
  EXPECT_EQ(setup.assets->json(*path + ":valid"), Json(41));
  ByteArray invalid(64, 'x');
  auto binary = binaryPath(invalid.span());
  ASSERT_TRUE(binary);
  std::array<std::string, 3> originalErrors;
  for (unsigned attempt = 0; attempt != 2; ++attempt) {
    auto badImage = setup.assets->image(*binary);
    auto badAudio = setup.assets->audio(*binary);
    auto badFont = setup.assets->font(*binary);
    ASSERT_FALSE(badImage);
    ASSERT_FALSE(badAudio);
    ASSERT_FALSE(badFont);
    EXPECT_TRUE(badImage.error().is<ImageException>());
    EXPECT_TRUE(badAudio.error().is<AudioException>());
    EXPECT_TRUE(badFont.error().is<FontException>());
    std::array<std::string, 3> errors{
      badImage.error().what(), badAudio.error().what(), badFont.error().what()};
    if (attempt == 0)
      originalErrors = errors;
    else
      EXPECT_EQ(errors, originalErrors) << "Warm error caches must retain native type and diagnostic";
    auto raw = setup.assets->bytes(*binary, true);
    ASSERT_TRUE(raw);
    EXPECT_EQ(**raw, invalid);
  }
  auto pixels = embeddedPixels();
  ASSERT_TRUE(pixels);
  ASSERT_TRUE(setup.lua->setPath("recoveredImage", *pixels));
  auto validImagePath = setup.lua->eval<String>("return recoveredImage:embed('Zstd')");
  ASSERT_TRUE(validImagePath);
  auto recoveredImage = setup.assets->image(*validImagePath);
  ASSERT_TRUE(recoveredImage) << recoveredImage.error().what();
  expectPixels(**recoveredImage, *pixels);
});

const GameTestRegistration embeddedScriptBoundary("EmbeddedAssetsTest", "RootCallbacksAndScriptLoadingKeepSeparatePermissions", [](GameTest& fixture) {
  RandomService random(0x534352495054);
  auto directory = File::temporary_directory(random);
  ASSERT_TRUE(directory);
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  ASSERT_TRUE(File::write_file(String("embeddedControl = 17"), File::relative_to(*directory, "embedded-control.lua")));
  auto settings = embeddedRootSettings(fixture, *directory, true);
  ASSERT_TRUE(settings) << settings.error().what();
  auto root = Root::create(std::move(*settings));
  ASSERT_TRUE(root) << root.error().what();
  auto lua = embeddedRootLua(**root);
  ASSERT_TRUE(lua) << lua.error().what();
  auto context = (*lua)->createContext(StringList{});
  ASSERT_TRUE(context) << context.error().what();
  auto result = context->eval<Json>(R"LUA(
    local data = 'a\0b\255:;?/%'
    local path = sb.embedData(data, 'Zstd')
    local json = sb.embedJson({nested = {answer = 42}}, 'Zlib')
    local deniedBefore = not pcall(root.assetData, path)
    local explicit = root.assetData(path, true)
    local deniedAfter = not pcall(root.assetData, path)
    return {deniedBefore = deniedBefore, exact = explicit == data, deniedAfter = deniedAfter,
      answer = root.assetJson(json .. ':nested.answer')}
  )LUA");
  ASSERT_TRUE(result) << result.error().what();
  EXPECT_EQ(*result, Json(JsonObject{{"deniedBefore", true}, {"exact", true}, {"deniedAfter", true}, {"answer", 42}}));
  auto script = context->eval<String>("return sb.embedText('embeddedExecuted = true')");
  ASSERT_TRUE(script);
  auto assets = (*root)->assets();
  ASSERT_TRUE(assets);
  auto warmed = (*assets)->bytes(*script, true);
  ASSERT_TRUE(warmed) << warmed.error().what();
  EXPECT_EQ(**warmed, ByteArray::from_string("embeddedExecuted = true"));
  ASSERT_TRUE(context->setPath("embeddedScript", *script));
  auto requireResult = context->eval<Json>(R"LUA(
    require('/embedded-control.lua')
    local ok = pcall(require, embeddedScript)
    return {control = embeddedControl, rejected = not ok, untouched = embeddedExecuted == nil}
  )LUA");
  ASSERT_TRUE(requireResult) << requireResult.error().what();
  EXPECT_EQ(*requireResult, Json(JsonObject{{"control", 17}, {"rejected", true}, {"untouched", true}}));
  auto rejectedContext = (*lua)->createContext(StringList{*script});
  ASSERT_FALSE(rejectedContext);
  EXPECT_TRUE(rejectedContext.error().is<LuaException>() || rejectedContext.error().is<AssetException>());
  auto recoveredContext = (*lua)->createContext(StringList{"/embedded-control.lua"});
  ASSERT_TRUE(recoveredContext) << recoveredContext.error().what();
  EXPECT_EQ(recoveredContext->eval<int>("return embeddedControl"), 17);
});

const GameTestRegistration embeddedDisabled("EmbeddedAssetsTest", "SafeConfigurationDisablesRawAndAllTypedConsumers", [](GameTest& fixture) {
  EmbeddedFixture setup;
  auto initialized = setup.init(fixture, false);
  ASSERT_TRUE(initialized) << initialized.error().what();
  auto path = setup.lua->eval<String>("return sb.embedText('{}')");
  ASSERT_TRUE(path);
  auto expectDenied = [](auto result) {
    ASSERT_FALSE(result);
    EXPECT_TRUE(result.error().template is<AssetException>());
    EXPECT_FALSE(std::string_view(result.error().what()).empty());
  };
  expectDenied(setup.assets->bytes(*path));
  expectDenied(setup.assets->bytes(*path, true));
  expectDenied(setup.assets->json(*path));
  expectDenied(setup.assets->image(*path));
  expectDenied(setup.assets->audio(*path));
  expectDenied(setup.assets->font(*path));
  auto settings = embeddedRootSettings(fixture, setup.directory, false);
  ASSERT_TRUE(settings) << settings.error().what();
  auto root = Root::create(std::move(*settings));
  ASSERT_TRUE(root) << root.error().what();
  auto lua = embeddedRootLua(**root);
  ASSERT_TRUE(lua) << lua.error().what();
  auto context = (*lua)->createContext(StringList{});
  ASSERT_TRUE(context) << context.error().what();
  auto result = context->eval<Json>(R"LUA(
    local path = sb.embedJson({answer = 42}, 'None')
    return {json = not pcall(root.assetJson, path), bytes = not pcall(root.assetData, path, true),
      ordinary = type(root.assetJson('/client.config')) == 'table'}
  )LUA");
  ASSERT_TRUE(result) << result.error().what();
  EXPECT_EQ(*result, Json(JsonObject{{"json", true}, {"bytes", true}, {"ordinary", true}}));
});

const GameTestRegistration embeddedEscapedFrameNames(
  "EmbeddedAssetsTest", "TextFramesPreserveQuotedBackslashAndNulNames", [](GameTest& fixture) {
    RandomService random(0x4652414d454e414d);
    auto directory = File::temporary_directory(random);
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto settings = embeddedRootSettings(fixture, *directory, true);
    ASSERT_TRUE(settings) << settings.error().what();
    auto root = Root::create(std::move(*settings));
    ASSERT_TRUE(root) << root.error().what();
    auto lua = embeddedRootLua(**root);
    ASSERT_TRUE(lua) << lua.error().what();
    auto context = (*lua)->createContext(StringList{});
    ASSERT_TRUE(context) << context.error().what();
    auto image = embeddedPixels();
    ASSERT_TRUE(image);
    ASSERT_TRUE(context->setPath("image", *image));
    auto expected = image->sub_image({1, 0}, {1, 2});
    ASSERT_TRUE(expected);
    auto assets = (*root)->assets();
    ASSERT_TRUE(assets);
    for (String const& name : {String("quote\"and\\backslash"), String(std::string("nul\0frame", 9))}) {
      ASSERT_TRUE(context->setPath("frameName", name));
      auto path = context->eval<String>(R"LUA(
        return image:embed('Zstd', {frameList = {[frameName] = {1, 0, 2, 2}}}, 'text')
      )LUA");
      ASSERT_TRUE(path) << path.error().what();
      AssetPath components;
      components.basePath = *path;
      components.subPath = name;
      auto loaded = (*assets)->image(components);
      ASSERT_TRUE(loaded) << loaded.error().what();
      expectPixels(**loaded, *expected);
      if (name == "quote\"and\\backslash") {
        ASSERT_TRUE(context->setPath("embeddedImage", *path));
        auto luaImage = context->eval<Image>("return root.assetImage(embeddedImage .. ':' .. frameName)");
        ASSERT_TRUE(luaImage) << luaImage.error().what();
        expectPixels(*luaImage, *expected);
      }
    }
  });

const GameTestRegistration embeddedPngBudget(
  "EmbeddedAssetsTest", "PngDecodedPixelBudgetRejectsBeforePixelAllocation", [](GameTest& fixture) {
    EmbeddedFixture setup;
    auto initialized = setup.init(fixture);
    ASSERT_TRUE(initialized) << initialized.error().what();
    auto image = embeddedPixels();
    ASSERT_TRUE(image);
    auto buffer = std::make_shared<Buffer>();
    ASSERT_TRUE(image->write_png(setup.logger, buffer));
    auto png = buffer->take_data();
    ASSERT_GE(png.size(), 33u);
    // A tiny valid PNG container with a CRC-valid, over-budget IHDR. Its IDAT
    // stays tiny: the diagnostic must be the size guard, not truncated pixels.
    auto write32 = [&](std::size_t offset, std::uint32_t value) {
      for (unsigned byte = 0; byte != 4; ++byte)
        png[offset + byte] = static_cast<char>(value >> (24 - byte * 8));
    };
    write32(16, 4097);
    write32(20, 4096);
    std::uint32_t crc = 0xffffffff;
    for (std::size_t offset = 12; offset != 29; ++offset) {
      crc ^= static_cast<unsigned char>(png[offset]);
      for (unsigned bit = 0; bit != 8; ++bit)
        crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    write32(29, ~crc);
    auto path = binaryPath(png.span(), EmbeddedCompressionMethod::Zlib);
    ASSERT_TRUE(path) << path.error().what();
    auto raw = setup.assets->bytes(*path, true);
    ASSERT_TRUE(raw);
    EXPECT_EQ(**raw, png);
    auto loaded = setup.assets->image(*path);
    ASSERT_FALSE(loaded);
    EXPECT_TRUE(loaded.error().is<AssetException>());
    EXPECT_NE(std::string_view(loaded.error().what()).find("64 MiB"), std::string_view::npos);
    auto valid = setup.lua->eval<String>("return sb.embedText('{\"after\":true}')");
    ASSERT_TRUE(valid);
    EXPECT_EQ(setup.assets->json(*valid + ":after"), Json(true));
  });
