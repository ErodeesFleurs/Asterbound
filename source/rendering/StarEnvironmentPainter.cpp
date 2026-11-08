import std;
import star.json;
import star.xx_hash;
import star.format;
import star.json_extra;
import star.id_map;
import star.color;
import star.list;
import star.rect;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.vector;
import star.bi_map;
import star.image;
import star.poly;
import star.game_types;
import star.version;
import star.string;
import star.asset_path;
import star.either;
import star.random;
import star.weighted_pool;
import star.casting;
import star.strong_typedef;
import star.listener;
import star.perlin;
import star.random_point;
import star.lexical_cast;
import star.time;
import star.logging;
import star.math_common;
import star.set;
import star.worker_pool;

import star.sector_array_2d;

import star.tile_damage;
import star.plant_database;
import star.parallax;
import star.collision_block;
import star.liquid_types;
import star.tile_sector_array;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.drawable;
import star.entity_rendering_types;
import star.sky_types;
import star.celestial_coordinate;
import star.sky_parameters;
import star.sky_render_data;
import star.animation;
import star.particle;
import star.weather_types;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.cellular_light_array;
import star.cellular_lighting;
import star.world_render_data;
import star.ref_ptr;
import star.renderer;
import star.asset_texture_group;
import star.environment_painter;
import star.root;

import star.world_camera;

import star.static_random;
import star.matrix3;
namespace Star {

float const EnvironmentPainter::SunriseTime = 0.072f;
float const EnvironmentPainter::SunsetTime = 0.42f;
float const EnvironmentPainter::SunFadeRate = 0.07f;
float const EnvironmentPainter::MaxFade = 0.3f;
float const EnvironmentPainter::RayPerlinFrequency = 0.005f; // Arbitrary, part of using the Perlin as a PRNG
float const EnvironmentPainter::RayPerlinAmplitude = 2;
int   const EnvironmentPainter::RayCount = 60;
float const EnvironmentPainter::RayMinWidth = 0.8f; // % of its sector
float const EnvironmentPainter::RayWidthVariance = 5.0265f; // % of its sector
float const EnvironmentPainter::RayAngleVariance = 6.2832f; // Radians
float const EnvironmentPainter::SunRadius = 50;
float const EnvironmentPainter::RayColorDependenceLevel = 3.0f;
float const EnvironmentPainter::RayColorDependenceScale = 0.00625f;
float const EnvironmentPainter::RayUnscaledAlphaVariance = 2.0943f;
float const EnvironmentPainter::RayMinUnscaledAlpha = 1;
Vec3B const EnvironmentPainter::RayColor = Vec3B(255, 255, 200);

EnvironmentPainter::EnvironmentPainter(Root& root, std::shared_ptr<Renderer> renderer)
  : m_root(root), m_renderer(std::move(renderer)), m_timer(0),
    m_rayPerlin(1, RayPerlinFrequency, RayPerlinAmplitude, 0, 2.0f, 2.0f, root.presentationRandom().randu64()) {}

std::expected<std::shared_ptr<EnvironmentPainter>, StarException> EnvironmentPainter::create(Root& root, std::shared_ptr<Renderer> renderer) {
  auto textureGroup = renderer->createTextureGroup(TextureGroupSize::Large);
  if (!textureGroup) return std::unexpected(std::move(textureGroup.error()));
  auto painter = std::shared_ptr<EnvironmentPainter>(new EnvironmentPainter(root, std::move(renderer)));
  painter->m_textureGroup = std::make_shared<AssetTextureGroup>(root, std::move(*textureGroup));
  return painter;
}

void EnvironmentPainter::update(float dt) {
  // Allows the rays to change alpha with time.
  m_timer += dt;
  m_timer = std::fmod(m_timer, std::numbers::pi * 100000.0);
}

std::expected<void, StarException> EnvironmentPainter::renderStars(float pixelRatio, Vec2F const& screenSize, SkyRenderData const& sky) {
  float nightSkyAlpha = 1.0f - std::min(sky.dayLevel, sky.skyAlpha);
  if (nightSkyAlpha <= 0.0f)
    return {};

  Vec4B color(255, 255, 255, 255 * nightSkyAlpha);

  Vec2F viewSize = screenSize / pixelRatio;
  Vec2F viewCenter = viewSize / 2;
  Vec2F viewMin = sky.starOffset - viewCenter;

  auto newStarsHash = starsHash(sky, viewSize);
  if (newStarsHash != m_starsHash || !m_starGenerator) {
    if (auto result = setupStars(sky); !result) return std::unexpected(std::move(result.error()));
    m_starsHash = newStarsHash;
  }

  if (!m_starGenerator || !sky.settings || sky.starFrames == 0 || sky.starTypes().empty())
    return {};

  auto screenBufferResult = sky.settings.queryFloat("stars.screenBuffer");
  if (!screenBufferResult) return std::unexpected(std::move(screenBufferResult.error()));
  float screenBuffer = *screenBufferResult;

  PolyF field = PolyF(RectF::withSize(viewMin, Vec2F(viewSize)).padded(screenBuffer));
  field.rotate(-sky.starRotation, Vec2F(sky.starOffset));

  Mat3F transform = Mat3F::identity();
  transform.translate(-viewMin);
  transform.rotate(sky.starRotation, viewCenter);

  auto twinkleMin = sky.settings.queryInt("stars.twinkleMin");
  if (!twinkleMin) return std::unexpected(std::move(twinkleMin.error()));
  auto twinkleMax = sky.settings.queryInt("stars.twinkleMax");
  if (!twinkleMax) return std::unexpected(std::move(twinkleMax.error()));
  std::size_t starTypesSize = sky.starTypes().size();

  auto stars = m_starGenerator->generate(field, [&](RandomSource& rand) -> std::expected<std::pair<std::size_t, float>, StarException> {
      std::size_t starType = rand.randu32() % starTypesSize;
      auto frame = rand.randu32() % sky.starFrames;
      auto twinkle = rand.randf(*twinkleMin, *twinkleMax);
      if (!twinkle) return std::unexpected(std::move(twinkle.error()));
      return std::pair<std::size_t, float>(starType, frame + *twinkle);
    });
  if (!stars) return std::unexpected(std::move(stars.error()));

  RectF viewRect = RectF::withSize(Vec2F(), viewSize).padded(screenBuffer);

  auto& primitives = m_renderer->immediatePrimitives();

  for (auto& star : *stars) {
    Vec2F screenPos = transform.transformVec2(star.first);
    if (viewRect.contains(screenPos)) {
      std::size_t starFrame = (std::size_t)(sky.epochTime + star.second.second) % sky.starFrames;
      if (auto const& texture = m_starTextures[star.second.first * sky.starFrames + starFrame])
        primitives.emplace_back(std::in_place_type_t<RenderQuad>(), texture, screenPos * pixelRatio - Vec2F(texture->size()) / 2, 1.0, color, 0.0f);
    }
  }

  return m_renderer->flush();
}

std::expected<void, StarException> EnvironmentPainter::renderDebrisFields(float pixelRatio, Vec2F const& screenSize, SkyRenderData const& sky) {
  if (!sky.settings || m_debrisGenerators.empty())
    return {};
  auto debrisSkyTypes = sky.settings.optArray("debrisSkyTypes");
  if (!debrisSkyTypes) return std::unexpected(std::move(debrisSkyTypes.error()));
  if (*debrisSkyTypes) {
    if (!SkyTypeNames) return std::unexpected(SkyTypeNames.error());
    auto skyType = SkyTypeNames->get_right(sky.type);
    if (!skyType) return std::unexpected(std::move(skyType.error()));
    StringList allowedTypes;
    for (auto const& type : **debrisSkyTypes) {
      auto name = type.toString();
      if (!name) return std::unexpected(std::move(name.error()));
      allowedTypes.push_back(std::move(*name));
    }
    auto allowed = allowedTypes.contains(skyType->get());
    if (!allowed) return std::unexpected(std::move(allowed.error()));
    if (!*allowed)
      return {};
  }
  Vec2F viewSize = screenSize / pixelRatio;
  Vec2F viewCenter = viewSize / 2;
  Vec2D viewMin = Vec2D(sky.starOffset - viewCenter);
  Mat3F rotMatrix = Mat3F::rotation(sky.starRotation, viewCenter);
  auto debrisFields = sky.settings.queryArray("spaceDebrisFields");
  if (!debrisFields) return std::unexpected(std::move(debrisFields.error()));
  for (std::size_t i = 0; i < debrisFields->size(); ++i) {
    Json const& debrisField = (*debrisFields)[i];
    auto velocityJson = debrisField.query("velocityRange");
    if (!velocityJson) return std::unexpected(std::move(velocityJson.error()));
    auto velocityRange = jsonToVec2F(*velocityJson);
    if (!velocityRange) return std::unexpected(std::move(velocityRange.error()));
    auto debrisXVel =
      static_random_float_range((*velocityRange)[0], (*velocityRange)[1], sky.skyParameters.seed, i, "DebrisFieldXVel");
    auto debrisYVel =
      static_random_float_range((*velocityRange)[0], (*velocityRange)[1], sky.skyParameters.seed, i, "DebrisFieldYVel");
    Vec2D velocityOffset = -Vec2D(debrisXVel, debrisYVel) * sky.epochTime;
    auto imageOptions = debrisField.queryArray("list");
    if (!imageOptions) return std::unexpected(std::move(imageOptions.error()));
    Vec2U biggest;
    for (Json const& json : *imageOptions) {
      auto image = json.toString();
      if (!image) return std::unexpected(std::move(image.error()));
      auto texture = m_textureGroup->loadTexture(*image);
      if (!texture) return std::unexpected(std::move(texture.error()));
      biggest = biggest.piecewise_max((*texture)->size());
    }
    float screenBuffer = std::ceil((float)biggest.max() * (float)std::numbers::sqrt2);
    PolyD field = PolyD(RectD::withSize(viewMin + velocityOffset, Vec2D(viewSize)).padded(screenBuffer));
    auto angularJson = debrisField.query("angularVelocityRange");
    if (!angularJson) return std::unexpected(std::move(angularJson.error()));
    auto angularRange = jsonToVec2F(*angularJson);
    if (!angularRange) return std::unexpected(std::move(angularRange.error()));
    auto debrisItems = m_debrisGenerators[i]->generate(field,
        [&](RandomSource& rand) -> std::expected<std::pair<String, float>, StarException> {
          auto option = rand.randFrom(*imageOptions);
          if (!option) return std::unexpected(std::move(option.error()));
          auto image = option->get().toString();
          if (!image) return std::unexpected(std::move(image.error()));
          auto angularVelocity = rand.randf((*angularRange)[0], (*angularRange)[1]);
          if (!angularVelocity) return std::unexpected(std::move(angularVelocity.error()));
          return std::pair<String, float>(std::move(*image), *angularVelocity);
        });
    if (!debrisItems) return std::unexpected(std::move(debrisItems.error()));
    Vec2D debrisPositionOffset = viewMin + velocityOffset;
    for (auto& debrisItem : *debrisItems) {
      Vec2F debrisPosition = rotMatrix.transformVec2(Vec2F(debrisItem.first - debrisPositionOffset));
      float debrisAngle = std::fmod(Constants::deg2rad * debrisItem.second.second * sky.epochTime, std::numbers::pi * 2) + sky.starRotation;
      if (auto result = drawOrbiter(pixelRatio, screenSize, sky, {SkyOrbiterType::SpaceDebris, 1.0f, debrisAngle, debrisItem.second.first, debrisPosition}); !result)
        return std::unexpected(std::move(result.error()));
    }
  }
  return m_renderer->flush();
}

std::expected<void, StarException> EnvironmentPainter::renderBackOrbiters(float pixelRatio, Vec2F const& screenSize, SkyRenderData const& sky) {
  auto orbiters = sky.backOrbiters(screenSize / pixelRatio);
  if (!orbiters) return std::unexpected(std::move(orbiters.error()));
  for (auto const& orbiter : *orbiters) {
    if (auto result = drawOrbiter(pixelRatio, screenSize, sky, orbiter); !result)
      return std::unexpected(std::move(result.error()));
  }
  return m_renderer->flush();
}

std::expected<void, StarException> EnvironmentPainter::renderPlanetHorizon(float pixelRatio, Vec2F const& screenSize, SkyRenderData const& sky) {
  auto horizon = sky.worldHorizon(screenSize / pixelRatio);
  if (!horizon) return std::unexpected(std::move(horizon.error()));
  auto const& planetHorizon = *horizon;
  if (planetHorizon.empty())
    return {};

  // Can't bail sooner, need to queue all textures
  bool allLoaded = true;
  for (auto const& layer : planetHorizon.layers) {
    auto left = m_textureGroup->tryTexture(layer.first);
    if (!left) return std::unexpected(std::move(left.error()));
    auto right = m_textureGroup->tryTexture(layer.second);
    if (!right) return std::unexpected(std::move(right.error()));
    if (!*left || !*right)
      allLoaded = false;
  }

  if (!allLoaded)
    return {};

  float planetPixelRatio = pixelRatio * planetHorizon.scale;
  Vec2F center = planetHorizon.center * pixelRatio;

  auto& primitives = m_renderer->immediatePrimitives();
  auto drawHorizonHalf = [&](TexturePtr texture, bool leftHalf) {
    Vec2F textureSize(texture->size());
    Vec2F layerPosition = center;
    if (leftHalf)
      layerPosition[0] -= textureSize[0] * planetPixelRatio;
    auto rect = RectF::withSize(layerPosition, textureSize * planetPixelRatio);
    PolyF image = PolyF(rect);
    image.rotate(planetHorizon.rotation, center);

    primitives.emplace_back(std::in_place_type_t<RenderQuad>(), std::move(texture), image[0], Vec2F(0, 0), image[1],
                            Vec2F(textureSize[0], 0), image[2], Vec2F(textureSize[0], textureSize[1]), image[3],
                            Vec2F(0, textureSize[1]), Vec4B::filled(255), 0.0f);
  };

  for (auto const& layer : planetHorizon.layers) {
    auto left = m_textureGroup->loadTexture(layer.first);
    if (!left) return std::unexpected(std::move(left.error()));
    auto right = m_textureGroup->loadTexture(layer.second);
    if (!right) return std::unexpected(std::move(right.error()));
    drawHorizonHalf(std::move(*left), true);
    drawHorizonHalf(std::move(*right), false);
  }

  return m_renderer->flush();
}

std::expected<void, StarException> EnvironmentPainter::renderFrontOrbiters(float pixelRatio, Vec2F const& screenSize, SkyRenderData const& sky) {
  auto orbiters = sky.frontOrbiters(screenSize / pixelRatio);
  if (!orbiters) return std::unexpected(std::move(orbiters.error()));
  for (auto const& orbiter : *orbiters) {
    if (auto result = drawOrbiter(pixelRatio, screenSize, sky, orbiter); !result)
      return std::unexpected(std::move(result.error()));
  }
  return m_renderer->flush();
}

std::expected<void, StarException> EnvironmentPainter::renderSky(Vec2F const& screenSize, SkyRenderData const& sky) {
  auto& primitives = m_renderer->immediatePrimitives();
  primitives.emplace_back(std::in_place_type_t<RenderQuad>(), TexturePtr(),
      RenderVertex{Vec2F(0, 0), Vec2F(), sky.bottomRectColor.toRgba(), 0.0f},
      RenderVertex{Vec2F(screenSize[0], 0), Vec2F(), sky.bottomRectColor.toRgba(), 0.0f},
      RenderVertex{screenSize, Vec2F(), sky.topRectColor.toRgba(), 0.0f},
      RenderVertex{Vec2F(0, screenSize[1]), Vec2F(), sky.topRectColor.toRgba(), 0.0f});

  // Flash overlay for Interstellar travel
  Vec4B flashColor = sky.flashColor.toRgba();
  primitives.emplace_back(std::in_place_type_t<RenderQuad>(), RectF(Vec2F(), screenSize), flashColor, 0.0f);

  return m_renderer->flush();
}

// TODO: Fix this to work with decimal zoom levels. Currently, the clouds shake rapidly when interpolating between zoom levels.
std::expected<void, StarException> EnvironmentPainter::renderParallaxLayers(
    Vec2F parallaxWorldPosition, WorldCamera const& camera, ParallaxLayers const& layers, SkyRenderData const& sky) {

  // Note: the "parallax space" referenced below is a grid where the scale of each cell is the size of the parallax image

  auto& primitives = m_renderer->immediatePrimitives();

  for (auto& layer : layers) {
    if (layer.alpha == 0)
      continue;

    Vec4B drawColor;
    if (layer.unlit || layer.lightMapped)
      drawColor = Vec4B(255, 255, 255, std::floor(255 * layer.alpha));
    else
      drawColor = Vec4B(sky.environmentLight.toRgb(), std::floor(255 * layer.alpha));

    Vec2F parallaxValue = layer.parallaxValue;
    Vec2B parallaxRepeat = layer.repeat;
    Vec2F parallaxOrigin = {0.0f, layer.verticalOrigin};

    auto firstTexture = layer.textures.first();
    if (!firstTexture) return std::unexpected(std::move(firstTexture.error()));
    AssetPath first = firstTexture->get();
    first.directives += layer.directives;
    if (layer.frameNumber > 1)
      first.subPath.emplace("0");
    auto parallaxTexture = m_textureGroup->loadTexture(first);
    if (!parallaxTexture) return std::unexpected(std::move(parallaxTexture.error()));
    Vec2F parallaxSize = Vec2F((*parallaxTexture)->size());
    Vec2F parallaxPixels = parallaxSize * camera.pixelRatio();

    // texture offset in *screen pixel space*
    Vec2F parallaxOffset = layer.parallaxOffset * camera.pixelRatio();
    if (layer.speed[0] != 0) {
      double drift = std::fmod((double)layer.speed[0] * (sky.epochTime / (double)sky.dayLength), (double)parallaxSize[0]);
      parallaxOffset[0] = std::fmod(parallaxOffset[0] + drift * camera.pixelRatio(), parallaxPixels[0]);
    }
    if (layer.speed[1] != 0) {
      double drift = std::fmod((double)layer.speed[1] * (sky.epochTime / (double)sky.dayLength), (double)parallaxSize[1]);
      parallaxOffset[1] = std::fmod(parallaxOffset[1] + drift * camera.pixelRatio(), parallaxPixels[1]);
    }

    // parallax camera world position in *parallax space*
    Vec2F parallaxCameraCenter = parallaxWorldPosition - parallaxOrigin;
    parallaxCameraCenter =
        Vec2F((((parallaxCameraCenter[0] / parallaxPixels[0]) * TilePixels) * camera.pixelRatio()) / parallaxValue[0],
            (((parallaxCameraCenter[1] / parallaxPixels[1]) * TilePixels) * camera.pixelRatio()) / parallaxValue[1]);

    // width / height of screen in *parallax space*
    float parallaxScreenWidth = camera.screenSize()[0] / parallaxPixels[0];
    float parallaxScreenHeight = camera.screenSize()[1] / parallaxPixels[1];

    // screen world position in *parallax space*
    float parallaxScreenLeft = parallaxCameraCenter[0] - parallaxScreenWidth / 2.0;
    float parallaxScreenBottom = parallaxCameraCenter[1] - parallaxScreenHeight / 2.0;

    // screen boundary world positions in *parallax space*
    Vec2F parallaxScreenOffset = parallaxOffset.piecewise_divide(parallaxPixels);
    int left = std::floor(parallaxScreenLeft + parallaxScreenOffset[0]);
    int bottom = std::floor(parallaxScreenBottom + parallaxScreenOffset[1]);
    int right = std::ceil(parallaxScreenLeft + parallaxScreenWidth + parallaxScreenOffset[0]);
    int top = std::ceil(parallaxScreenBottom + parallaxScreenHeight + parallaxScreenOffset[1]);

    // positions to start tiling in *screen pixel space*
    float pixelLeft = (left - parallaxScreenLeft) * parallaxPixels[0] - parallaxOffset[0];
    float pixelBottom = (bottom - parallaxScreenBottom) * parallaxPixels[1] - parallaxOffset[1];

    // vertical top and bottom cutoff points in *parallax space*
    float tileLimitTop = top;
    if (layer.tileLimitTop)
      tileLimitTop = (layer.parallaxOffset[1] - layer.tileLimitTop.value_or(0.0f)) / parallaxSize[1];
    float tileLimitBottom = bottom;
    if (layer.tileLimitBottom)
      tileLimitBottom = (layer.parallaxOffset[1] - layer.tileLimitBottom.value_or(0.0f)) / parallaxSize[1];

    float lightMapMultiplier = (!layer.unlit && layer.lightMapped) ? 1.0f : 0.0f;

    for (int y = bottom; y <= top; ++y) {
      if (!(parallaxRepeat[1] || y == 0) || y > tileLimitTop || y + 1 < tileLimitBottom)
        continue;
      for (int x = left; x <= right; ++x) {
        if (!(parallaxRepeat[0] || x == 0))
          continue;
        float pixelTileLeft = pixelLeft + (x - left) * parallaxPixels[0];
        float pixelTileBottom = pixelBottom + (y - bottom) * parallaxPixels[1];

        Vec2F anchorPoint(pixelTileLeft, pixelTileBottom);

        RectF subImage = RectF::withSize(Vec2F(), parallaxSize);
        if (tileLimitTop != top && y + 1 > tileLimitTop)
          subImage.setYMin(parallaxSize[1] * (1.0f - fpart(tileLimitTop)));
        if (tileLimitBottom != bottom && y < tileLimitBottom)
          anchorPoint[1] += fpart(tileLimitBottom) * parallaxPixels[1];

        for (auto const& textureImage : layer.textures) {
          AssetPath withDirectives = textureImage;
          withDirectives.directives += layer.directives;
          if (layer.frameNumber > 1) {
            float time_within_cycle = (float)std::fmod(sky.epochTime, (double)layer.animationCycle);
            float time_per_frame = layer.animationCycle / layer.frameNumber;
            float frame_number = time_within_cycle / time_per_frame;
            int frame = (layer.frameOffset + clamp<int>(frame_number, 0, layer.frameNumber - 1)) % layer.frameNumber;
            withDirectives.subPath.emplace(toString(frame));
          }
          auto texture = m_textureGroup->tryTexture(withDirectives);
          if (!texture) return std::unexpected(std::move(texture.error()));
          if (*texture) {
            RectF drawRect = RectF::withSize(anchorPoint, subImage.size() * camera.pixelRatio());
            primitives.emplace_back(std::in_place_type_t<RenderQuad>(), std::move(*texture),
                RenderVertex{drawRect.min(), subImage.min(), drawColor, lightMapMultiplier},
                RenderVertex{{drawRect.xMax(), drawRect.yMin()}, {subImage.xMax(), subImage.yMin()}, drawColor, lightMapMultiplier},
                RenderVertex{drawRect.max(), subImage.max(), drawColor, lightMapMultiplier},
                RenderVertex{{drawRect.xMin(), drawRect.yMax()}, {subImage.xMin(), subImage.yMax()}, drawColor, lightMapMultiplier});
          }
        }
      }
    }
  }

  return m_renderer->flush();
}

void EnvironmentPainter::cleanup(std::int64_t textureTimeout) {
  m_textureGroup->cleanup(textureTimeout);
}

std::expected<void, StarException> EnvironmentPainter::drawRays(
    float pixelRatio, SkyRenderData const& sky, Vec2F start, float length, double time, float alpha) {
  // All magic constants are either 2PI or arbritrary to allow the Perlin to act
  // as a PRNG
  float sectorWidth = 2 * std::numbers::pi / RayCount; // Radians
  Vec3B color = sky.topRectColor.toRgb();

  for (int i = 0; i < RayCount; i++) {
    auto widthNoise = m_rayPerlin.get(i * 25);
    if (!widthNoise) return std::unexpected(std::move(widthNoise.error()));
    auto angleNoise = m_rayPerlin.get(i * 314);
    if (!angleNoise) return std::unexpected(std::move(angleNoise.error()));
    auto result = drawRay(pixelRatio,
        sky,
        start,
        sectorWidth * (std::abs(*widthNoise) * RayWidthVariance + RayMinWidth),
        length,
        i * sectorWidth + *angleNoise * RayAngleVariance,
        time,
        color,
        alpha);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  return m_renderer->flush();
}

std::expected<void, StarException> EnvironmentPainter::drawRay(float pixelRatio,
    SkyRenderData const& sky,
    Vec2F start,
    float width,
    float length,
    float angle,
    double time,
    Vec3B color,
    float alpha) {
  // All magic constants are arbritrary to allow the Perlin to act as a PRNG

  float currentTime = sky.timeOfDay / sky.dayLength;
  float timeSinceSunEvent = std::min(std::abs(currentTime - SunriseTime), std::abs(currentTime - SunsetTime));
  float percentFaded = MaxFade * (1.0f - std::min(1.0f, std::pow(timeSinceSunEvent / SunFadeRate, 2.0f)));
  // Gets the current average sky color
  color = (Vec3B)((Vec3F)color * (1 - percentFaded) + (Vec3F)sky.mainSkyColor.toRgb() * percentFaded);
  // Sum is used to vary the ray intensity based on sky color
  // Rays show up more on darker backgrounds, so this scales to remove that
  float sum = std::pow((color[0] + color[1]) * RayColorDependenceScale, RayColorDependenceLevel);
  auto dynamicImage = sky.settings.queryBool("sun.dynamicImage.enabled", false);
  if (!dynamicImage) return std::unexpected(std::move(dynamicImage.error()));
  auto rayColorJson = sky.settings.query("sun.rayColor", JsonArray{RayColor[0], RayColor[1], RayColor[2]});
  if (!rayColorJson) return std::unexpected(std::move(rayColorJson.error()));
  if (*dynamicImage && !sky.skyParameters.sunType.empty()) {
    auto dynamicColor = sky.settings.query("sun.dynamicImage.rayColors." + sky.skyParameters.sunType, *rayColorJson);
    if (!dynamicColor) return std::unexpected(std::move(dynamicColor.error()));
    rayColorJson = std::move(dynamicColor);
  }
  auto rayColorResult = jsonToVec3B(*rayColorJson);
  if (!rayColorResult) return std::unexpected(std::move(rayColorResult.error()));
  Vec3B rayColor = *rayColorResult;
  auto scale = sky.settings.queryFloat("sun.scale", 1.0f);
  if (!scale) return std::unexpected(std::move(scale.error()));
  float sunScale = *scale;
  auto firstNoise = m_rayPerlin.get(angle * 896 + time * 30);
  if (!firstNoise) return std::unexpected(std::move(firstNoise.error()));
  auto secondNoise = m_rayPerlin.get(angle * 626 + time * 30);
  if (!secondNoise) return std::unexpected(std::move(secondNoise.error()));
  m_renderer->immediatePrimitives().emplace_back(std::in_place_type_t<RenderQuad>(), TexturePtr(),
      RenderVertex{start + Vec2F(std::cos(angle + width), std::sin(angle + width)) * length, {}, Vec4B(rayColor, 0), 0.0f},
      RenderVertex{start + Vec2F(std::cos(angle + width), std::sin(angle + width)) * SunRadius * sunScale * pixelRatio,
          {},
          Vec4B(rayColor,
              (int)(RayMinUnscaledAlpha + std::abs(*firstNoise * RayUnscaledAlphaVariance))
                  * sum
                  * alpha), 0.0f},
      RenderVertex{start + Vec2F(std::cos(angle), std::sin(angle)) * SunRadius * sunScale * pixelRatio,
          {},
          Vec4B(rayColor,
              (int)(RayMinUnscaledAlpha + std::abs(*secondNoise * RayUnscaledAlphaVariance))
                  * sum
                  * alpha), 0.0f},
      RenderVertex{start + Vec2F(std::cos(angle), std::sin(angle)) * length, {}, Vec4B(rayColor, 0), 0.0f});
  return {};
}

std::expected<void, StarException> EnvironmentPainter::drawOrbiter(float pixelRatio, Vec2F const& screenSize, SkyRenderData const& sky, SkyOrbiter const& orbiter) {
  float alpha = 1.0f;
  Vec2F position;

  // The way Starbound positions these is weird.
  // It's a random point on a 400 by 400 area from the bottom left of the screen.
  // That origin point is then multiplied by the zoom level.
  // This does not intuitively scale with higher-resolution monitors, so lets fix that.
  if (orbiter.type == SkyOrbiterType::Moon) {
    const Vec2F correctionOrigin = { 320, 180 }; 
    // correctionOrigin is 1920x1080 / default zoom level / 2, the most likely dev setup at the time.
    Vec2F offset = orbiter.position - correctionOrigin;
    position = (screenSize / 2) + offset * pixelRatio;
  }
  else
    position = orbiter.position * pixelRatio;

  if (orbiter.type == SkyOrbiterType::Sun) {
    alpha = sky.dayLevel;
    if (auto result = drawRays(pixelRatio, sky, position, std::max(screenSize[0], screenSize[1]), m_timer, sky.skyAlpha); !result)
      return std::unexpected(std::move(result.error()));
  }

  auto loadedTexture = m_textureGroup->loadTexture(orbiter.image);
  if (!loadedTexture) return std::unexpected(std::move(loadedTexture.error()));
  TexturePtr texture = std::move(*loadedTexture);
  Vec2F texSize = Vec2F(texture->size());

  Mat3F renderMatrix = Mat3F::rotation(orbiter.angle, position);
  RectF renderRect = RectF::withCenter(position, texSize * orbiter.scale * pixelRatio);
  Vec4B renderColor = Vec4B(255, 255, 255, 255 * alpha);

  m_renderer->immediatePrimitives().emplace_back(std::in_place_type_t<RenderQuad>(), std::move(texture),
      renderMatrix.transformVec2(renderRect.min()), Vec2F(0, 0),
      renderMatrix.transformVec2(Vec2F{renderRect.xMax(), renderRect.yMin()}), Vec2F(texSize[0], 0),
      renderMatrix.transformVec2(renderRect.max()), Vec2F(texSize[0], texSize[1]),
      renderMatrix.transformVec2(Vec2F{renderRect.xMin(), renderRect.yMax()}), Vec2F(0, texSize[1]),
    renderColor, 0.0f);
  return {};
}

std::uint64_t EnvironmentPainter::starsHash(SkyRenderData const& sky, Vec2F const& viewSize) const {
  XXHash64 hasher;

  hasher.push(std::span<char const>(reinterpret_cast<char const*>(&viewSize[0]), sizeof(viewSize[0])));
  hasher.push(std::span<char const>(reinterpret_cast<char const*>(&viewSize[1]), sizeof(viewSize[1])));
  hasher.push(std::span<char const>(reinterpret_cast<char const*>(&sky.skyParameters.seed), sizeof(sky.skyParameters.seed)));
  hasher.push(std::span<char const>(reinterpret_cast<char const*>(&sky.type), sizeof(sky.type)));

  return hasher.digest();
}

std::expected<void, StarException> EnvironmentPainter::setupStars(SkyRenderData const& sky) {
  if (!sky.settings)
    return {};
  StringList const& starTypes = sky.starTypes();
  std::size_t starTypesSize = starTypes.size();
  List<TexturePtr> textures;
  textures.resize(starTypesSize * sky.starFrames);
  for (std::size_t i = 0; i < starTypesSize; ++i) {
    for (std::size_t j = 0; j < sky.starFrames; ++j) {
      auto texture = m_textureGroup->loadTexture(starTypes[i] + ":" + toString(j));
      if (!texture) return std::unexpected(std::move(texture.error()));
      textures[i * sky.starFrames + j] = std::move(*texture);
    }
  }
  auto starCellSize = sky.settings.queryInt("stars.cellSize");
  if (!starCellSize) return std::unexpected(std::move(starCellSize.error()));
  auto starCountJson = sky.settings.query("stars.cellCount");
  if (!starCountJson) return std::unexpected(std::move(starCountJson.error()));
  auto starCount = jsonToVec2I(*starCountJson);
  if (!starCount) return std::unexpected(std::move(starCount.error()));
  auto starGenerator = std::make_shared<Random2dPointGenerator<std::pair<std::size_t, float>>>(m_root.presentationRandom(), sky.skyParameters.seed, *starCellSize, *starCount);
  auto debrisFields = sky.settings.queryArray("spaceDebrisFields");
  if (!debrisFields) return std::unexpected(std::move(debrisFields.error()));
  decltype(m_debrisGenerators) debrisGenerators;
  debrisGenerators.resize(debrisFields->size());
  for (std::size_t i = 0; i < debrisFields->size(); ++i) {
    auto cellSize = (*debrisFields)[i].getInt("cellSize");
    if (!cellSize) return std::unexpected(std::move(cellSize.error()));
    auto countJson = (*debrisFields)[i].get("cellCountRange");
    if (!countJson) return std::unexpected(std::move(countJson.error()));
    auto count = jsonToVec2I(*countJson);
    if (!count) return std::unexpected(std::move(count.error()));
    std::uint64_t seed = static_random_u64(sky.skyParameters.seed, i, "DebrisFieldSeed");
    debrisGenerators[i] = std::make_shared<Random2dPointGenerator<std::pair<String, float>, double>>(m_root.presentationRandom(), seed, *cellSize, *count);
  }
  m_starTextures = std::move(textures);
  m_starGenerator = std::move(starGenerator);
  m_debrisGenerators = std::move(debrisGenerators);
  return {};
}

}
