import star.json;
import star.json_extra;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.color;
import star.vector;
import star.bi_map;
import star.lexical_cast;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.string;
import star.format;
import star.version;
import star.random;
import star.data_stream_extra;

import star.tile_damage;
import star.plant_database;
import star.parallax;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;


import star.image_metadata_database;

import star.algorithm;
namespace Star {

ParallaxLayer::ParallaxLayer() {
  timeOfDayCorrelation = "";
  zLevel = 0;
  verticalOrigin = 0;
  speed = { 0, 0 };
  followsWind = false;
  unlit = false;
  lightMapped = false;
  fadePercent = 0;
  directives = "";
  frameNumber = 1;
  frameOffset = 0;
  animationCycle = 1.0f;
  alpha = 1.0f;
}

std::expected<ParallaxLayer, StarException> ParallaxLayer::create(Json const& store) {
  ParallaxLayer result;
  auto textures = store.get("textures");
  if (!textures) return std::unexpected(std::move(textures.error()));
  auto textureList = jsonToStringList(*textures);
  if (!textureList) return std::unexpected(std::move(textureList.error()));
  result.textures = std::move(*textureList);
  auto directivesResult = store.getString("directives");
  if (!directivesResult) return std::unexpected(std::move(directivesResult.error()));
  result.directives = std::move(*directivesResult);
  auto frameNumberResult = store.getInt("frameNumber", 1);
  if (!frameNumberResult) return std::unexpected(std::move(frameNumberResult.error()));
  result.frameNumber = std::move(*frameNumberResult);
  auto frameOffsetResult = store.getInt("frameOffset", 0);
  if (!frameOffsetResult) return std::unexpected(std::move(frameOffsetResult.error()));
  result.frameOffset = std::move(*frameOffsetResult);
  auto animationCycleResult = store.getFloat("animationCycle", 1.0f);
  if (!animationCycleResult) return std::unexpected(std::move(animationCycleResult.error()));
  result.animationCycle = std::move(*animationCycleResult);
  auto tileLimitTopResult = store.optFloat("tileLimitTop");
  if (!tileLimitTopResult) return std::unexpected(std::move(tileLimitTopResult.error()));
  result.tileLimitTop = std::move(*tileLimitTopResult);
  auto tileLimitBottomResult = store.optFloat("tileLimitBottom");
  if (!tileLimitBottomResult) return std::unexpected(std::move(tileLimitBottomResult.error()));
  result.tileLimitBottom = std::move(*tileLimitBottomResult);
  auto verticalOriginResult = store.getFloat("verticalOrigin");
  if (!verticalOriginResult) return std::unexpected(std::move(verticalOriginResult.error()));
  result.verticalOrigin = std::move(*verticalOriginResult);
  auto zLevelResult = store.getFloat("zLevel");
  if (!zLevelResult) return std::unexpected(std::move(zLevelResult.error()));
  result.zLevel = std::move(*zLevelResult);
  auto timeOfDayCorrelationResult = store.getString("timeOfDayCorrelation");
  if (!timeOfDayCorrelationResult) return std::unexpected(std::move(timeOfDayCorrelationResult.error()));
  result.timeOfDayCorrelation = std::move(*timeOfDayCorrelationResult);
  auto followsWindResult = store.getBool("followsWind", false);
  if (!followsWindResult) return std::unexpected(std::move(followsWindResult.error()));
  result.followsWind = std::move(*followsWindResult);
  auto windSpeedMultiplierResult = store.optFloat("windSpeedMultiplier");
  if (!windSpeedMultiplierResult) return std::unexpected(std::move(windSpeedMultiplierResult.error()));
  result.windSpeedMultiplier = std::move(*windSpeedMultiplierResult);
  auto unlitResult = store.getBool("unlit");
  if (!unlitResult) return std::unexpected(std::move(unlitResult.error()));
  result.unlit = std::move(*unlitResult);
  auto lightMappedResult = store.getBool("lightMapped");
  if (!lightMappedResult) return std::unexpected(std::move(lightMappedResult.error()));
  result.lightMapped = std::move(*lightMappedResult);
  auto fadePercentResult = store.getFloat("fadePercent");
  if (!fadePercentResult) return std::unexpected(std::move(fadePercentResult.error()));
  result.fadePercent = std::move(*fadePercentResult);
  auto parallaxValueJson = store.get("parallaxValue");
  if (!parallaxValueJson) return std::unexpected(std::move(parallaxValueJson.error()));
  auto parallaxValueResult = jsonToVec2F(*parallaxValueJson);
  if (!parallaxValueResult) return std::unexpected(std::move(parallaxValueResult.error()));
  result.parallaxValue = std::move(*parallaxValueResult);
  auto repeatJson = store.get("repeat");
  if (!repeatJson) return std::unexpected(std::move(repeatJson.error()));
  auto repeatResult = jsonToVec2B(*repeatJson);
  if (!repeatResult) return std::unexpected(std::move(repeatResult.error()));
  result.repeat = std::move(*repeatResult);
  auto parallaxOffsetJson = store.get("parallaxOffset");
  if (!parallaxOffsetJson) return std::unexpected(std::move(parallaxOffsetJson.error()));
  auto parallaxOffsetResult = jsonToVec2F(*parallaxOffsetJson);
  if (!parallaxOffsetResult) return std::unexpected(std::move(parallaxOffsetResult.error()));
  result.parallaxOffset = std::move(*parallaxOffsetResult);
  auto speedX = store.getFloat("speed");
  if (!speedX) return std::unexpected(std::move(speedX.error()));
  auto speedY = store.getFloat("speedY", 0.0f);
  if (!speedY) return std::unexpected(std::move(speedY.error()));
  result.speed = {*speedX, *speedY};
  return result;
}

Json ParallaxLayer::store() const {
  return JsonObject{
      {"textures", jsonFromStringList(textures)},
      {"directives", directives.string()},
      {"frameNumber", frameNumber},
      {"frameOffset", frameOffset},
      {"animationCycle", animationCycle},
      {"parallaxValue", jsonFromVec2F(parallaxValue)},
      {"repeat", jsonFromVec2B(repeat)},
      {"tileLimitTop", jsonFromOptional(tileLimitTop)},
      {"tileLimitBottom", jsonFromOptional(tileLimitBottom)},
      {"verticalOrigin", verticalOrigin},
      {"zLevel", zLevel},
      {"parallaxOffset", jsonFromVec2F(parallaxOffset)},
      {"timeOfDayCorrelation", timeOfDayCorrelation},
      {"speed",  speed[0]},
      {"speedY", speed[1]},
      {"followsWind", followsWind},
      {"windSpeedMultiplier", jsonFromOptional(windSpeedMultiplier)},
      {"unlit", unlit},
      {"lightMapped", lightMapped},
      {"fadePercent", fadePercent}};
}

void ParallaxLayer::addImageDirectives(Directives const& newDirectives) {
  if (newDirectives) { // TODO: Move to Directives +=
    if (directives) {
      String dirString = directives.string();

      auto newStringLease = newDirectives.maybeString();
      auto const& newString = newStringLease->get();
      if (!newString.empty()) {
        if (newString.utf8().front() != '?')
          dirString += "?";
        dirString += newString;
      }

      directives = std::move(dirString);
    }
    else
      directives = newDirectives;
  }
}

std::expected<void, StarException> ParallaxLayer::fadeToSkyColor(Color skyColor) {
  if (fadePercent > 0) {
    auto hex = skyColor.toHex().slice(0, 6);
    if (!hex) return std::unexpected(std::move(hex.error()));
    String fade = "fade=" + *hex + "=" + toString(fadePercent);
    addImageDirectives(fade);
  }
  return {};
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, ParallaxLayer& parallaxLayer) {
  if (auto r = ds.read(parallaxLayer.textures); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(parallaxLayer.directives); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(parallaxLayer.alpha); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(parallaxLayer.parallaxValue); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(parallaxLayer.repeat); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(parallaxLayer.tileLimitTop); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(parallaxLayer.tileLimitBottom); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(parallaxLayer.verticalOrigin); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(parallaxLayer.zLevel); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(parallaxLayer.parallaxOffset); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(parallaxLayer.timeOfDayCorrelation); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(parallaxLayer.speed); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(parallaxLayer.followsWind); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(parallaxLayer.windSpeedMultiplier); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(parallaxLayer.unlit); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(parallaxLayer.lightMapped); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(parallaxLayer.fadePercent); !r) return std::unexpected(std::move(r.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, ParallaxLayer const& parallaxLayer) {
  if (auto r = ds.write(parallaxLayer.textures); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(parallaxLayer.directives); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(parallaxLayer.alpha); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(parallaxLayer.parallaxValue); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(parallaxLayer.repeat); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(parallaxLayer.tileLimitTop); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(parallaxLayer.tileLimitBottom); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(parallaxLayer.verticalOrigin); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(parallaxLayer.zLevel); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(parallaxLayer.parallaxOffset); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(parallaxLayer.timeOfDayCorrelation); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(parallaxLayer.speed); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(parallaxLayer.followsWind); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(parallaxLayer.windSpeedMultiplier); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(parallaxLayer.unlit); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(parallaxLayer.lightMapped); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(parallaxLayer.fadePercent); !r) return std::unexpected(std::move(r.error()));
  return std::ref(ds);
}

std::expected<std::shared_ptr<Parallax>, StarException> Parallax::create(Root& root, String const& assetFile,
    std::uint64_t seed, float verticalOrigin, float hueShift, std::optional<TreeVariant> parallaxTreeVariant) {
  auto result = std::shared_ptr<Parallax>(new Parallax(root));
  result->m_seed = seed;
  result->m_verticalOrigin = verticalOrigin;
  result->m_parallaxTreeVariant = std::move(parallaxTreeVariant);
  result->m_hueShift = hueShift;
  result->m_imageDirectory = "/parallax/images/";
  auto assets = root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json(assetFile);
  if (!config) return std::unexpected(std::move(config.error()));
  auto origin = config->getFloat("verticalOrigin", 0);
  if (!origin) return std::unexpected(std::move(origin.error()));
  result->m_verticalOrigin += *origin;
  RandomSource rnd(seed);
  auto layers = config->getArray("layers");
  if (!layers) return std::unexpected(std::move(layers.error()));
  for (auto const& settings : *layers) {
    auto kind = settings.getString("kind");
    if (!kind) return std::unexpected(std::move(kind.error()));
    auto frequency = settings.getFloat("frequency", 1.0);
    if (!frequency) return std::unexpected(std::move(frequency.error()));
    if (rnd.randf() > *frequency) continue;
    if (auto r = result->buildLayer(settings, *kind); !r) return std::unexpected(std::move(r.error()));
  }
  std::stable_sort(result->m_layers.begin(), result->m_layers.end(), [](ParallaxLayer const& a, ParallaxLayer const& b) { return a.zLevel > b.zLevel; });
  return result;
}

std::expected<std::shared_ptr<Parallax>, StarException> Parallax::create(Root& root, Json const& store) {
  auto result = std::shared_ptr<Parallax>(new Parallax(root));
  auto m_seedResult = store.getUInt("seed");
  if (!m_seedResult) return std::unexpected(std::move(m_seedResult.error()));
  result->m_seed = std::move(*m_seedResult);
  auto m_verticalOriginResult = store.getFloat("verticalOrigin");
  if (!m_verticalOriginResult) return std::unexpected(std::move(m_verticalOriginResult.error()));
  result->m_verticalOrigin = std::move(*m_verticalOriginResult);
  auto m_hueShiftResult = store.getFloat("hueShift");
  if (!m_hueShiftResult) return std::unexpected(std::move(m_hueShiftResult.error()));
  result->m_hueShift = std::move(*m_hueShiftResult);
  auto m_imageDirectoryResult = store.getString("imageDirectory");
  if (!m_imageDirectoryResult) return std::unexpected(std::move(m_imageDirectoryResult.error()));
  result->m_imageDirectory = std::move(*m_imageDirectoryResult);
  auto tree = store.opt("parallaxTreeVariant");
  if (!tree) return std::unexpected(std::move(tree.error()));
  if (*tree) {
    auto assets = root.assets();
    if (!assets) return std::unexpected(std::move(assets.error()));
    auto variant = TreeVariant::create(**assets, **tree);
    if (!variant) return std::unexpected(std::move(variant.error()));
    result->m_parallaxTreeVariant = std::move(*variant);
  }
  auto layers = store.getArray("layers");
  if (!layers) return std::unexpected(std::move(layers.error()));
  for (auto const& layer : *layers) {
    auto loaded = ParallaxLayer::create(layer);
    if (!loaded) return std::unexpected(std::move(loaded.error()));
    result->m_layers.push_back(std::move(*loaded));
  }
  std::stable_sort(result->m_layers.begin(), result->m_layers.end(), [](ParallaxLayer const& a, ParallaxLayer const& b) { return a.zLevel > b.zLevel; });
  return result;
}

std::expected<Json, StarException> Parallax::store() const {
  Json tree;
  if (m_parallaxTreeVariant) {
    auto stored = m_parallaxTreeVariant->toJson();
    if (!stored) return std::unexpected(std::move(stored.error()));
    tree = std::move(*stored);
  }
  return JsonObject{{"seed", m_seed},
      {"verticalOrigin", m_verticalOrigin},
      {"parallaxTreeVariant", std::move(tree)},
      {"hueShift", m_hueShift},
      {"imageDirectory", m_imageDirectory},
      {"layers", m_layers.transformed(std::mem_fn(&ParallaxLayer::store))}};
}

std::expected<void, StarException> Parallax::fadeToSkyColor(Color const& skyColor) {
  for (auto& layer : m_layers) {
    if (auto r = layer.fadeToSkyColor(skyColor); !r) return std::unexpected(std::move(r.error()));
  }
  return {};
}

std::expected<std::shared_ptr<Parallax>, StarException> Parallax::createOverlay(String const& assetFile) const {
  return Parallax::create(m_root, assetFile, m_seed, m_verticalOrigin, m_hueShift, m_parallaxTreeVariant);
}

ParallaxLayers const& Parallax::layers() const {
  return m_layers;
}

std::expected<void, StarException> Parallax::buildLayer(Json const& layerSettings, String const& kind) {
  auto isFoliage = kind.beginsWith("foliage/");
  if (!isFoliage)
    return std::unexpected(std::move(isFoliage.error()));
  auto isStem = kind.beginsWith("stem/");
  if (!isStem)
    return std::unexpected(std::move(isStem.error()));

  String texPath = m_imageDirectory + kind + "/";
  if (*isFoliage) {
    // If our tree type has no foliage, don't construct this layer.
    if (!m_parallaxTreeVariant)
      return {};
    auto enabled = m_parallaxTreeVariant->foliageSettings.getBool("parallaxFoliage", false);
    if (!enabled)
      return std::unexpected(std::move(enabled.error()));
    if (!*enabled)
      return {};
    auto subdirectory = kind.replace("foliage/", "");
    if (!subdirectory)
      return std::unexpected(std::move(subdirectory.error()));
    texPath = m_parallaxTreeVariant->foliageDirectory + "parallax/" + *subdirectory + "/";
  } else if (*isStem) {
    if (!m_parallaxTreeVariant)
      return {};
    auto subdirectory = kind.replace("stem/", "");
    if (!subdirectory)
      return std::unexpected(std::move(subdirectory.error()));
    texPath = m_parallaxTreeVariant->stemDirectory + "parallax/" + *subdirectory + "/";
  }

  ParallaxLayer layer;
  RandomSource rnd(m_seed + m_layers.size());
  auto imgMetadata = m_root.imageMetadataDatabase();
  if (!imgMetadata)
    return std::unexpected(std::move(imgMetadata.error()));

  auto baseCount = layerSettings.getInt("baseCount", 1);
  if (!baseCount)
    return std::unexpected(std::move(baseCount.error()));
  int baseCountValue = *baseCount;
  int base = rnd.randInt(baseCountValue - 1) + 1;
  layer.textures.push_back(AssetPath::relative_to(texPath, "base/" + toString<int>(base) + ".png"));
  auto modCount = layerSettings.getInt("modCount", 0);
  if (!modCount)
    return std::unexpected(std::move(modCount.error()));
  int modCountValue = *modCount;
  int mod = rnd.randInt(modCountValue);
  if (mod != 0)
    layer.textures.push_back(AssetPath::relative_to(texPath, "mod/" + toString<int>(mod) + ".png"));

  auto parallaxConfig = layerSettings.get("parallax");
  if (!parallaxConfig)
    return std::unexpected(std::move(parallaxConfig.error()));
  if (parallaxConfig->type() == Json::Type::Array) {
    auto value = jsonToVec2F(*parallaxConfig);
    if (!value)
      return std::unexpected(std::move(value.error()));
    layer.parallaxValue = *value;
  } else {
    auto value = parallaxConfig->toFloat();
    if (!value)
      return std::unexpected(std::move(value.error()));
    layer.parallaxValue = Vec2F::filled(*value);
  }

  auto repeatY = layerSettings.getBool("repeatY", false);
  if (!repeatY)
    return std::unexpected(std::move(repeatY.error()));
  layer.repeat = {true, *repeatY};
  if (*repeatY) {
    auto top = layerSettings.optFloat("tileLimitTop");
    if (!top)
      return std::unexpected(std::move(top.error()));
    layer.tileLimitTop = *top;
    auto bottom = layerSettings.optFloat("tileLimitBottom");
    if (!bottom)
      return std::unexpected(std::move(bottom.error()));
    layer.tileLimitBottom = *bottom;
  }

  layer.verticalOrigin = m_verticalOrigin;
  layer.zLevel = layer.parallaxValue.sum();
  auto offset = layerSettings.getArray("offset", {0, 0});
  if (!offset)
    return std::unexpected(std::move(offset.error()));
  auto offsetX = offset->at(0);
  if (!offsetX)
    return std::unexpected(std::move(offsetX.error()));
  auto x = offsetX->get().toFloat();
  if (!x)
    return std::unexpected(std::move(x.error()));
  auto offsetY = offset->at(1);
  if (!offsetY)
    return std::unexpected(std::move(offsetY.error()));
  auto y = offsetY->get().toFloat();
  if (!y)
    return std::unexpected(std::move(y.error()));
  layer.parallaxOffset = {*x, *y};
  auto noRandomOffset = layerSettings.getBool("noRandomOffset", false);
  if (!noRandomOffset)
    return std::unexpected(std::move(noRandomOffset.error()));
  if (!*noRandomOffset) {
    auto size = (*imgMetadata)->imageSize(layer.textures[0]);
    if (!size)
      return std::unexpected(std::move(size.error()));
    layer.parallaxOffset[0] += rnd.randInt((*size)[0]);
  }
  auto correlation = layerSettings.getString("timeOfDayCorrelation", "");
  if (!correlation)
    return std::unexpected(std::move(correlation.error()));
  layer.timeOfDayCorrelation = std::move(*correlation);
  auto minSpeed = layerSettings.get("minSpeed", 0.0f);
  if (!minSpeed)
    return std::unexpected(std::move(minSpeed.error()));
  auto maxSpeed = layerSettings.get("maxSpeed", 0.0f);
  if (!maxSpeed)
    return std::unexpected(std::move(maxSpeed.error()));
  if (!minSpeed->isType(Json::Type::Array))
    *minSpeed = JsonArray{*minSpeed, 0.0f};
  if (!maxSpeed->isType(Json::Type::Array))
    *maxSpeed = JsonArray{*maxSpeed, 0.0f};
  auto minX = minSpeed->getFloat(0);
  if (!minX)
    return std::unexpected(std::move(minX.error()));
  auto maxX = maxSpeed->getFloat(0);
  if (!maxX)
    return std::unexpected(std::move(maxX.error()));
  auto speedX = rnd.randf(*minX, *maxX);
  if (!speedX)
    return std::unexpected(std::move(speedX.error()));
  auto minY = minSpeed->getFloat(1);
  if (!minY)
    return std::unexpected(std::move(minY.error()));
  auto maxY = maxSpeed->getFloat(1);
  if (!maxY)
    return std::unexpected(std::move(maxY.error()));
  auto speedY = rnd.randf(*minY, *maxY);
  if (!speedY)
    return std::unexpected(std::move(speedY.error()));
  layer.speed = {*speedX, *speedY};
  auto followsWind = layerSettings.getBool("followsWind", false);
  if (!followsWind)
    return std::unexpected(std::move(followsWind.error()));
  layer.followsWind = *followsWind;
  auto windSpeedMultiplier = layerSettings.optFloat("windSpeedMultiplier");
  if (!windSpeedMultiplier)
    return std::unexpected(std::move(windSpeedMultiplier.error()));
  layer.windSpeedMultiplier = *windSpeedMultiplier;
  auto unlit = layerSettings.getBool("unlit", false);
  if (!unlit)
    return std::unexpected(std::move(unlit.error()));
  layer.unlit = *unlit;
  auto lightMapped = layerSettings.getBool("lightMapped", false);
  if (!lightMapped)
    return std::unexpected(std::move(lightMapped.error()));
  layer.lightMapped = *lightMapped;

  auto directives = layerSettings.getString("directives", "");
  if (!directives)
    return std::unexpected(std::move(directives.error()));
  layer.addImageDirectives(*directives);
  if (*isFoliage) {
    layer.addImageDirectives(String(strf("hueshift={}", m_parallaxTreeVariant->foliageHueShift)));
  } else if (*isStem) {
    layer.addImageDirectives(String(strf("hueshift={}", m_parallaxTreeVariant->stemHueShift)));
  } else {
    auto noHueShift = layerSettings.getBool("nohueshift", false);
    if (!noHueShift)
      return std::unexpected(std::move(noHueShift.error()));
    if (!*noHueShift)
      layer.addImageDirectives(String(strf("hueshift={}", m_hueShift)));
  }

  auto fadePercent = layerSettings.getFloat("fadePercent", 0);
  if (!fadePercent)
    return std::unexpected(std::move(fadePercent.error()));
  layer.fadePercent = *fadePercent;
  auto frameNumber = layerSettings.getInt("frameNumber", 1);
  if (!frameNumber)
    return std::unexpected(std::move(frameNumber.error()));
  layer.frameNumber = *frameNumber;
  auto frameOffset = layerSettings.getInt("frameOffset", 0);
  if (!frameOffset)
    return std::unexpected(std::move(frameOffset.error()));
  layer.frameOffset = *frameOffset;
  auto animationCycle = layerSettings.getFloat("animationCycle", 1.0f);
  if (!animationCycle)
    return std::unexpected(std::move(animationCycle.error()));
  layer.animationCycle = *animationCycle;

  m_layers.push_back(std::move(layer));
  return {};
}

}
