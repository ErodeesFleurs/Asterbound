import star.format;
import star.json;
import star.json_extra;
import star.id_map;
import star.string;
import star.data_stream;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.casting;
import star.vector;
import star.game_types;
import star.strong_typedef;
import star.bi_map;
import star.math_common;
import star.random;
import star.periodic;
import star.periodic_function;
import star.set;
import star.lua;
import star.audio;
import star.map;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.memory;
import star.version;


import star.lua_root;
import star.lua_components;
import star.lua_animation_component;
import star.tile_damage;
import star.interaction_types;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.status_effect_entity;
import star.scripted_entity;
import star.chat_action;
import star.chatty_entity;
import star.wiring;
import star.wire_entity;
import star.inspectable_entity;
import star.animated_part_set;
import star.animation;
import star.particle;
import star.mixer;
import star.networked_animator;
import star.entity_rendering;
import star.object;
import star.drawable;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.anchorable_entity;
import star.entity_rendering_types;
import star.lounging_entities;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

import star.loungeable_object;
import star.object_database;


namespace Star {

LoungeableObject::LoungeableObject(Root& rootContext) : Entity(rootContext), Object(rootContext), m_sitFlipDirection(false), m_sitOrientation{}, m_sitAngle(0), m_flipImages(false) {}

std::expected<std::shared_ptr<LoungeableObject>, StarException> LoungeableObject::create(Root& rootContext, std::shared_ptr<ObjectConfig const> config, Json const& parameters) {
  auto object = std::shared_ptr<LoungeableObject>(new LoungeableObject(rootContext));
  auto operation1 = object->initialize(std::move(config), parameters);
  if (!operation1)
    return std::unexpected(std::move(operation1.error()));
  object->m_interactive.set(true);
  return object;
}

std::expected<void, StarException> LoungeableObject::render(RenderCallback& renderCallback) {
  auto operation2 = Object::render(renderCallback);
  if (!operation2)
    return std::unexpected(std::move(operation2.error()));
  if (!m_sitCoverImage.empty()) {
    auto loungingResult = entitiesLounging();
    if (!loungingResult)
      return std::unexpected(std::move(loungingResult.error()));
    auto lounging = std::move(*loungingResult);
    if (!lounging.empty()) {
      if (auto orientation = currentOrientation()) {
        auto metadataResult = root().imageMetadataDatabase();
        if (!metadataResult)
          return std::unexpected(std::move(metadataResult.error()));
        auto metadata = std::move(*metadataResult);
        auto drawableResult = Drawable::makeImage(*metadata, m_sitCoverImage, 1.0f / TilePixels, false, position() + orientation->imagePosition);
        if (!drawableResult)
          return std::unexpected(std::move(drawableResult.error()));
        auto drawable = std::move(*drawableResult);
        if (m_flipImages) {
          auto boundsResult = drawable.boundBox(*metadata, false);
          if (!boundsResult)
            return std::unexpected(std::move(boundsResult.error()));
          auto bounds = std::move(*boundsResult);
          drawable.scale(Vec2F(-1, 1), bounds.center());
        }
        auto operation3 = renderCallback.addDrawable(std::move(drawable), RenderLayerObject + 2);
        if (!operation3)
          return std::unexpected(std::move(operation3.error()));
      }
    }
  }
  return {};
}

std::expected<InteractAction, StarException> LoungeableObject::interact(InteractRequest const& request) {
  auto resultResult = Object::interact(request);
  if (!resultResult)
    return std::unexpected(std::move(resultResult.error()));
  auto result = std::move(*resultResult);
  if (result.type == InteractActionType::None && !m_sitPositions.empty()) {
    std::optional<std::size_t> index;
    Vec2F offset = direction() == Direction::Right ? position() - request.interactPosition : request.interactPosition - position();
    for (std::size_t i = 0; i < m_sitPositions.size(); ++i) {
      if (!index || vmag(m_sitPositions[i] + offset) < vmag(m_sitPositions[*index] + offset))
        index = i;
    }
    return InteractAction(InteractActionType::SitDown, entityId(), *index);
  }
  return result;
}

std::size_t LoungeableObject::anchorCount() const {
  return m_sitPositions.size();
}

std::expected<std::shared_ptr<LoungeAnchor const>, StarException> LoungeableObject::loungeAnchor(std::size_t positionIndex) const {
  if (positionIndex >= m_sitPositions.size())
    return std::shared_ptr<LoungeAnchor const>();

  auto loungeAnchor = std::make_shared<LoungeAnchor>();

  loungeAnchor->suppressTools = false;
  loungeAnchor->controllable = false;
  loungeAnchor->direction = m_sitFlipDirection ? -direction() : direction();

  loungeAnchor->position = m_sitPositions[positionIndex];
  if (loungeAnchor->direction == Direction::Left)
    loungeAnchor->position[0] *= -1;
  loungeAnchor->position += position();

  loungeAnchor->exitBottomPosition = Vec2F(loungeAnchor->position[0], position()[1] + volume().boundBox().min()[1]);

  loungeAnchor->angle = m_sitAngle;
  if (loungeAnchor->direction == Direction::Left)
    loungeAnchor->angle *= -1;

  loungeAnchor->orientation = m_sitOrientation;
  // Layer all anchored entities one above the object layer, in top to bottom
  // order based on the anchor index.
  loungeAnchor->loungeRenderLayer = RenderLayerObject + m_sitPositions.size() - positionIndex;

  loungeAnchor->statusEffects = m_sitStatusEffects;
  loungeAnchor->effectEmitters = m_sitEffectEmitters;
  loungeAnchor->emote = m_sitEmote;
  loungeAnchor->dance = m_sitDance;
  loungeAnchor->armorCosmeticOverrides = m_sitArmorCosmeticOverrides;
  loungeAnchor->cursorOverride = m_sitCursorOverride;

  return loungeAnchor;
}

std::expected<void, StarException> LoungeableObject::setOrientationIndex(std::size_t orientationIndex) {
  auto operation1 = Object::setOrientationIndex(orientationIndex);
  if (!operation1)
    return std::unexpected(std::move(operation1.error()));
  if (orientationIndex != NPos) {
    auto positionConfigResult = configValue("sitPosition");
    if (!positionConfigResult)
      return std::unexpected(std::move(positionConfigResult.error()));
    auto positionConfig = std::move(*positionConfigResult);
    if (positionConfig) {
      auto positionResult = jsonToVec2F(positionConfig);
      if (!positionResult)
        return std::unexpected(std::move(positionResult.error()));
      auto position = std::move(*positionResult);
      m_sitPositions = {position / TilePixels};
    } else {
      auto positionsConfigResult = configValue("sitPositions");
      if (!positionsConfigResult)
        return std::unexpected(std::move(positionsConfigResult.error()));
      auto positionsConfig = std::move(*positionsConfigResult);
      if (positionsConfig) {
        auto positionsResult = positionsConfig.toArray();
        if (!positionsResult)
          return std::unexpected(std::move(positionsResult.error()));
        auto positions = std::move(*positionsResult);
        m_sitPositions.clear();
        for (auto const& value : positions) {
          auto positionResult = jsonToVec2F(value);
          if (!positionResult)
            return std::unexpected(std::move(positionResult.error()));
          auto position = std::move(*positionResult);
          m_sitPositions.push_back(position / TilePixels);
        }
      }
    }
    auto sitFlipDirectionConfigResult = configValue("sitFlipDirection", false);
    if (!sitFlipDirectionConfigResult)
      return std::unexpected(std::move(sitFlipDirectionConfigResult.error()));
    auto sitFlipDirectionConfig = std::move(*sitFlipDirectionConfigResult);
    auto sitFlipDirectionResult = sitFlipDirectionConfig.toBool();
    if (!sitFlipDirectionResult)
      return std::unexpected(std::move(sitFlipDirectionResult.error()));
    auto sitFlipDirection = std::move(*sitFlipDirectionResult);
    m_sitFlipDirection = std::move(sitFlipDirection);
    auto sitAngleConfigResult = configValue("sitAngle", 0);
    if (!sitAngleConfigResult)
      return std::unexpected(std::move(sitAngleConfigResult.error()));
    auto sitAngleConfig = std::move(*sitAngleConfigResult);
    auto sitAngleResult = sitAngleConfig.toFloat();
    if (!sitAngleResult)
      return std::unexpected(std::move(sitAngleResult.error()));
    auto sitAngle = std::move(*sitAngleResult);
    m_sitAngle = sitAngle * std::numbers::pi / 180.0f;
    auto sitCoverImageConfigResult = configValue("sitCoverImage", "");
    if (!sitCoverImageConfigResult)
      return std::unexpected(std::move(sitCoverImageConfigResult.error()));
    auto sitCoverImageConfig = std::move(*sitCoverImageConfigResult);
    auto sitCoverImageResult = sitCoverImageConfig.toString();
    if (!sitCoverImageResult)
      return std::unexpected(std::move(sitCoverImageResult.error()));
    auto sitCoverImage = std::move(*sitCoverImageResult);
    m_sitCoverImage = std::move(sitCoverImage);
    auto flipImagesConfigResult = configValue("flipImages", false);
    if (!flipImagesConfigResult)
      return std::unexpected(std::move(flipImagesConfigResult.error()));
    auto flipImagesConfig = std::move(*flipImagesConfigResult);
    auto flipImagesResult = flipImagesConfig.toBool();
    if (!flipImagesResult)
      return std::unexpected(std::move(flipImagesResult.error()));
    auto flipImages = std::move(*flipImagesResult);
    m_flipImages = std::move(flipImages);
    auto sitEmoteConfigResult = configValue("sitEmote");
    if (!sitEmoteConfigResult)
      return std::unexpected(std::move(sitEmoteConfigResult.error()));
    auto sitEmoteConfig = std::move(*sitEmoteConfigResult);
    auto sitEmoteResult = sitEmoteConfig.optString();
    if (!sitEmoteResult)
      return std::unexpected(std::move(sitEmoteResult.error()));
    auto sitEmote = std::move(*sitEmoteResult);
    m_sitEmote = std::move(sitEmote);
    auto sitDanceConfigResult = configValue("sitDance");
    if (!sitDanceConfigResult)
      return std::unexpected(std::move(sitDanceConfigResult.error()));
    auto sitDanceConfig = std::move(*sitDanceConfigResult);
    auto sitDanceResult = sitDanceConfig.optString();
    if (!sitDanceResult)
      return std::unexpected(std::move(sitDanceResult.error()));
    auto sitDance = std::move(*sitDanceResult);
    m_sitDance = std::move(sitDance);
    auto sitArmorCosmeticOverridesConfigResult = configValue("sitArmorCosmeticOverrides", JsonObject());
    if (!sitArmorCosmeticOverridesConfigResult)
      return std::unexpected(std::move(sitArmorCosmeticOverridesConfigResult.error()));
    auto sitArmorCosmeticOverridesConfig = std::move(*sitArmorCosmeticOverridesConfigResult);
    auto sitArmorCosmeticOverridesResult = sitArmorCosmeticOverridesConfig.toObject();
    if (!sitArmorCosmeticOverridesResult)
      return std::unexpected(std::move(sitArmorCosmeticOverridesResult.error()));
    auto sitArmorCosmeticOverrides = std::move(*sitArmorCosmeticOverridesResult);
    m_sitArmorCosmeticOverrides = std::move(sitArmorCosmeticOverrides);
    auto sitCursorOverrideConfigResult = configValue("sitCursorOverride");
    if (!sitCursorOverrideConfigResult)
      return std::unexpected(std::move(sitCursorOverrideConfigResult.error()));
    auto sitCursorOverrideConfig = std::move(*sitCursorOverrideConfigResult);
    auto sitCursorOverrideResult = sitCursorOverrideConfig.optString();
    if (!sitCursorOverrideResult)
      return std::unexpected(std::move(sitCursorOverrideResult.error()));
    auto sitCursorOverride = std::move(*sitCursorOverrideResult);
    m_sitCursorOverride = std::move(sitCursorOverride);
    auto orientationConfigResult = configValue("sitOrientation", "sit");
    if (!orientationConfigResult)
      return std::unexpected(std::move(orientationConfigResult.error()));
    auto orientationConfig = std::move(*orientationConfigResult);
    auto orientationNameResult = orientationConfig.toString();
    if (!orientationNameResult)
      return std::unexpected(std::move(orientationNameResult.error()));
    auto orientationName = std::move(*orientationNameResult);
    if (!LoungeOrientationNames)
      return std::unexpected(LoungeOrientationNames.error());
    auto orientationResult = LoungeOrientationNames->get_left(orientationName);
    if (!orientationResult)
      return std::unexpected(std::move(orientationResult.error()));
    auto orientation = std::move(*orientationResult);
    m_sitOrientation = orientation.get();
    auto effectsConfigResult = configValue("sitStatusEffects", JsonArray());
    if (!effectsConfigResult)
      return std::unexpected(std::move(effectsConfigResult.error()));
    auto effectsConfig = std::move(*effectsConfigResult);
    auto effectsResult = effectsConfig.toArray();
    if (!effectsResult)
      return std::unexpected(std::move(effectsResult.error()));
    auto effects = std::move(*effectsResult);
    List<PersistentStatusEffect> parsedEffects;
    for (auto const& effect : effects) {
      auto parsedResult = jsonToPersistentStatusEffect(effect);
      if (!parsedResult)
        return std::unexpected(std::move(parsedResult.error()));
      auto parsed = std::move(*parsedResult);
      parsedEffects.push_back(std::move(parsed));
    }
    m_sitStatusEffects = std::move(parsedEffects);
    auto emittersConfigResult = configValue("sitEffectEmitters", JsonArray());
    if (!emittersConfigResult)
      return std::unexpected(std::move(emittersConfigResult.error()));
    auto emittersConfig = std::move(*emittersConfigResult);
    auto emittersResult = jsonToStringSet(emittersConfig);
    if (!emittersResult)
      return std::unexpected(std::move(emittersResult.error()));
    auto emitters = std::move(*emittersResult);
    m_sitEffectEmitters = std::move(emitters);
  }
  return {};
}

}
