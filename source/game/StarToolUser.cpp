import star.json;
import star.id_map;
import star.data_stream;
import star.bi_map;
import star.game_types;
import star.strong_typedef;
import star.vector;
import star.color;
import star.string;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.casting;
import star.audio;
import star.map;
import star.set;
import std;
import star.format;
import star.rpc_promise;
import star.math_common;
import star.random;
import star.periodic;
import star.periodic_function;
import star.lua;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import star.version;
import star.a_star;
import star.spline;
import star.observer_stream;
import star.either;

import star.lua_root;
import star.lua_components;
import star.status_effect_entity;
import star.scripted_entity;
import star.chat_action;
import star.chatty_entity;
import star.wiring;
import star.wire_entity;
import star.inspectable_entity;
import star.animated_part_set;
import star.networked_animator;
import star.object;
import star.item;
import star.item_descriptor;
import star.item_recipe;
import star.humanoid;
import star.status_types;
import star.damage_types;
import star.world_geometry;
import star.damage;
import star.drawable;
import star.entity_rendering_types;
import star.entity;
import star.collision_block;
import star.force_regions;
import star.physics_entity;

import star.tool_user;
import star.asset_source;
import star.assets;
import star.configuration;
import star.root;
import star.status_effect_item;
import star.effect_source_item;
import star.previewable_item;
import star.movement_controller;
import star.platformer_astar_types;
import star.anchorable_entity;
import star.game_timers;
import star.actor_movement_controller;
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.tool_user_item;
import star.fireable_item;
import star.swingable_item;
import star.armors;
import star.mixer;
import star.animation;
import star.particle;
import star.entity_rendering;
import star.renderable_item;
import star.non_rotated_drawables_item;
import star.beam_item;
import star.durability_item;
import star.pointable_item;
import star.net_compatibility;
struct PreviewTile;
import star.preview_tile_tool;
import star.tools;
import star.activatable_item;
import star.tile_damage;
import star.interaction_types;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.tile_entity;
import star.tile_modification;
import star.world;
import star.lua_actor_movement_component;
import star.stat_collection;
import star.status_effect_database;
import star.status_controller;

import star.inspection_tool;
import star.active_item;
import star.flashlight;
import star.object_item;
import star.item_database;
import star.object_database;
import star.versioning_database;


import star.effect_emitter;

import star.light_source;

import star.net_element;

import star.algorithm;
namespace Star {

ToolUser::ToolUser(Root& root)
  : m_root(root), m_beamGunRadius(), m_beamGunGlowBorder(), m_objectPreviewInnerAlpha(), m_objectPreviewOuterAlpha(),
    m_user(), m_primaryHandItem(root), m_altHandItem(root),
    m_fireMain(), m_fireAlt(), m_edgeTriggeredMain(), m_edgeTriggeredAlt(), m_edgeSuppressedMain(), m_edgeSuppressedAlt(),
    m_suppress() {
  // Resource loading is performed by initialize(), after non-failing state setup.


  float fixedPointBase = 1.f / 60.f;
  m_primaryFireTimerNetState.setFixedPointBase(fixedPointBase);
  m_altFireTimerNetState.setFixedPointBase(fixedPointBase);
  m_primaryTimeFiringNetState.setFixedPointBase(fixedPointBase);
  m_altTimeFiringNetState.setFixedPointBase(fixedPointBase);

  auto interpolateTimer = [](double offset, double min, double max) -> double {
    if (max > min)
      return min + offset * (max - min);
    else
      return max;
  };

  m_primaryFireTimerNetState.setInterpolator(interpolateTimer);
  m_altFireTimerNetState.setInterpolator(interpolateTimer);
  m_primaryTimeFiringNetState.setInterpolator(interpolateTimer);
  m_altTimeFiringNetState.setInterpolator(interpolateTimer);
}

[[nodiscard]] std::expected<std::shared_ptr<ToolUser>, StarException> ToolUser::create(Root& root) {
  auto user = std::shared_ptr<ToolUser>(new ToolUser(root));
  auto initialized = user->initialize();
  if (!initialized) return std::unexpected(std::move(initialized.error()));
  return user;
}

[[nodiscard]] std::expected<void, StarException> ToolUser::initialize() {
  for (auto element : std::initializer_list<std::reference_wrapper<NetElement>>{
      m_primaryHandItem, m_altHandItem, m_primaryFireTimerNetState, m_altFireTimerNetState,
      m_primaryTimeFiringNetState, m_altTimeFiringNetState,
      m_primaryItemActiveNetState, m_altItemActiveNetState}) {
    auto added = addNetElement(element);
    if (!added) return std::unexpected(std::move(added.error()));
  }
  auto assets = m_root.get().assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/player.config");
  if (!config) return std::unexpected(std::move(config.error()));
  auto radius = config->getFloat("initialBeamGunRadius");
  if (!radius) return std::unexpected(std::move(radius.error()));
  auto border = config->getInt("previewGlowBorder");
  if (!border) return std::unexpected(std::move(border.error()));
  auto inner = config->getFloat("objectPreviewInnerAlpha");
  if (!inner) return std::unexpected(std::move(inner.error()));
  auto outer = config->getFloat("objectPreviewOuterAlpha");
  if (!outer) return std::unexpected(std::move(outer.error()));
  m_beamGunRadius = *radius;
  m_beamGunGlowBorder = *border;
  m_objectPreviewInnerAlpha = *inner;
  m_objectPreviewOuterAlpha = *outer;
  return {};
}

[[nodiscard]] std::expected<Json, StarException> ToolUser::diskStore() const {
  JsonObject data;
  auto versioning = m_root.get().versioningDatabase();
  if (!versioning) return std::unexpected(std::move(versioning.error()));
  for (auto const& entry : {std::pair{std::string_view("primaryHandItem"), m_primaryHandItem.get()}, {"altHandItem", m_altHandItem.get()}}) {
    if (entry.second) {
      auto stored = entry.second->descriptor().diskStore(**versioning);
      if (!stored) return std::unexpected(std::move(stored.error()));
      data[entry.first] = std::move(*stored);
    }
  }
  return data;
}

[[nodiscard]] std::expected<void, StarException> ToolUser::diskLoad(Json const& store) {
  auto database = m_root.get().itemDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  auto primaryJson = store.get("primaryHandItem", {});
  if (!primaryJson) return std::unexpected(std::move(primaryJson.error()));
  auto primary = (*database)->diskLoad(*primaryJson);
  if (!primary) return std::unexpected(std::move(primary.error()));
  auto altJson = store.get("altHandItem", {});
  if (!altJson) return std::unexpected(std::move(altJson.error()));
  auto alt = (*database)->diskLoad(*altJson);
  if (!alt) return std::unexpected(std::move(alt.error()));
  auto primarySet = m_primaryHandItem.set(std::move(*primary));
  if (!primarySet) return std::unexpected(std::move(primarySet.error()));
  return m_altHandItem.set(std::move(*alt));
}

std::shared_ptr<Item> ToolUser::primaryHandItem() const {
  return m_primaryHandItem.get();
}

std::shared_ptr<Item> ToolUser::altHandItem() const {
  return m_altHandItem.get();
}

ItemDescriptor ToolUser::primaryHandItemDescriptor() const {
  if (m_primaryHandItem.get())
    return m_primaryHandItem.get()->descriptor();
  return {};
}

ItemDescriptor ToolUser::altHandItemDescriptor() const {
  if (m_altHandItem.get())
    return m_altHandItem.get()->descriptor();
  return {};
}

[[nodiscard]] std::expected<void, StarException> ToolUser::init(ToolUserEntity& user) {
  m_user = std::ref(user);
  auto primary = initPrimaryHandItem();
  if (!primary) return std::unexpected(std::move(primary.error()));
  if (!itemSafeTwoHanded(m_primaryHandItem.get())) return initAltHandItem();
  return {};
}

[[nodiscard]] std::expected<void, StarException> ToolUser::uninit() {
  m_user.reset();
  auto primary = uninitItem(m_primaryHandItem.get());
  if (!primary) return std::unexpected(std::move(primary.error()));
  return uninitItem(m_altHandItem.get());
}

[[nodiscard]] std::expected<List<LightSource>, StarException> ToolUser::lightSources(float interpolation) const {
  if (m_suppress.get() || !m_user)
    return {};

  List<LightSource> lights;
  for (auto const& item : {m_primaryHandItem.get(), m_altHandItem.get()}) {
    if (auto active = as<ActiveItem>(item)) {
      auto sources = active->lights(interpolation);
      if (!sources) return std::unexpected(std::move(sources.error()));
      lights.appendAll(*sources);
    }
    if (auto flashlight = as<Flashlight>(item)) {
      auto sources = flashlight->lightSources();
      if (!sources) return std::unexpected(std::move(sources.error()));
      lights.appendAll(*sources);
    }
    if (auto inspection = as<InspectionTool>(item)) {
      auto sources = inspection->lightSources();
      if (!sources) return std::unexpected(std::move(sources.error()));
      lights.appendAll(*sources);
    }
  }

  return lights;
}

[[nodiscard]] std::expected<void, StarException> ToolUser::effects(EffectEmitter& emitter) const {
  if (m_suppress.get())
    return {};

  for (auto const& entry : {std::pair{std::string_view("primary"), m_primaryHandItem.get()}, {"alt", m_altHandItem.get()}}) {
    if (auto item = as<EffectSourceItem>(entry.second)) {
      auto sources = item->effectSources();
      if (!sources) return std::unexpected(std::move(sources.error()));
      emitter.addEffectSources(entry.first, std::move(*sources));
    }
  }
  return {};
}

[[nodiscard]] std::expected<List<PersistentStatusEffect>, StarException> ToolUser::statusEffects() const {
  if (m_suppress.get()) return List<PersistentStatusEffect>();
  List<PersistentStatusEffect> effects;
  for (auto const& item : {m_primaryHandItem.get(), m_altHandItem.get()}) {
    if (!is<ArmorItem>(item)) {
      if (auto source = as<StatusEffectItem>(item)) {
        auto status = source->statusEffects();
        if (!status) return std::unexpected(std::move(status.error()));
        effects.appendAll(*status);
      }
    }
  }
  return effects;
}

[[nodiscard]] std::expected<std::optional<float>, StarException> ToolUser::toolRadius() const {
  if (m_suppress.get()) return std::optional<float>();
  if (is<BeamItem>(m_primaryHandItem.get()) || is<BeamItem>(m_altHandItem.get())
      || is<WireTool>(m_primaryHandItem.get()) || is<WireTool>(m_altHandItem.get())) {
    auto radius = beamGunRadius();
    if (!radius) return std::unexpected(std::move(radius.error()));
    return std::optional<float>(*radius);
  }
  return std::optional<float>();
}

[[nodiscard]] std::expected<List<Drawable>, StarException> ToolUser::renderObjectPreviews(Vec2F aimPosition, Direction walkingDirection, bool inToolRange, Color favoriteColor) {
  if (m_suppress.get() || !m_user)
    return {};

  auto generate = [&](std::shared_ptr<ObjectItem> item) -> std::expected<List<Drawable>, StarException> {
    Vec2I aimPos(aimPosition.floor());
    if ((aimPos == m_cachedObjectPreviewPosition) && (item == m_cachedObjectItem))
      return m_cachedObjectPreview;

    auto objectDatabase = m_root.get().objectDatabase();
    if (!objectDatabase) return std::unexpected(std::move(objectDatabase.error()));
    auto world = m_user->get().world();
    if (!world) return std::unexpected(std::move(world.error()));
    auto objectName = item->objectName();
    if (!objectName) return std::unexpected(std::move(objectName.error()));
    auto objectParameters = item->objectParameters();
    if (!objectParameters) return std::unexpected(std::move(objectParameters.error()));
    auto drawablesResult = (*objectDatabase)->cursorHintDrawables(world->get(), *objectName,
        aimPos, walkingDirection, std::move(*objectParameters));
    if (!drawablesResult) return std::unexpected(std::move(drawablesResult.error()));
    auto drawables = std::move(*drawablesResult);

    Color opacityMask = Color::White;
    auto opacity = item->getAppropriateOpacity();
    if (!opacity) return std::unexpected(std::move(opacity.error()));
    opacityMask.setAlphaF(*opacity);
    Color favoriteColorTrans = favoriteColor;
    if (!inToolRange) favoriteColorTrans.setHue(favoriteColor.hue() + 120);
    else {
      auto placementObjectName = item->objectName();
      if (!placementObjectName) return std::unexpected(std::move(placementObjectName.error()));
      auto canPlace = (*objectDatabase)->canPlaceObject(world->get(), aimPos, *placementObjectName);
      if (!canPlace) return std::unexpected(std::move(canPlace.error()));
      if (!*canPlace) favoriteColorTrans.setHue(favoriteColor.hue() + 120);
    }

    favoriteColorTrans.setAlphaF(m_objectPreviewOuterAlpha);
    Color nearWhite = favoriteColorTrans;
    nearWhite.setValue(1 - (1 - nearWhite.value()) / 5);
    nearWhite.setSaturation(nearWhite.saturation() / 5);
    nearWhite.setAlphaF(m_objectPreviewInnerAlpha);
    ImageOperation op = BorderImageOperation{m_beamGunGlowBorder, nearWhite.toRgba(), favoriteColorTrans.toRgba(), false, false};

    auto imageMetadata = m_root.get().imageMetadataDatabase();
    if (!imageMetadata) return std::unexpected(std::move(imageMetadata.error()));
    auto operation = imageOperationToString(op);
    for (Drawable& drawable : drawables) {
      if (drawable.isImage()) {
        auto applied = drawable.imagePart().addDirectives(**imageMetadata, operation, true);
        if (!applied) return std::unexpected(std::move(applied.error()));
      }
      drawable.color = opacityMask;
    }
    m_cachedObjectPreview = drawables;
    m_cachedObjectItem = item;
    m_cachedObjectPreviewPosition = aimPos;
    return drawables;
  };

  if (auto pri = as<ObjectItem>(m_primaryHandItem.get()))
    return generate(pri);
  else if (auto alt = as<ObjectItem>(m_altHandItem.get()))
    return generate(alt);
  else
    m_cachedObjectItem.reset();
  return {};
}

void ToolUser::setupHumanoidHandItemPose(Humanoid& humanoid) const {
  humanoid.setHandFrameOverrides(ToolHand::Primary, "", "");
  humanoid.setHandFrameOverrides(ToolHand::Alt, "", "");
  if (m_suppress.get() || !m_user)
    return;
  auto setupHand = [&](bool primary) {
    auto const& item = primary ? m_primaryHandItem.get() : m_altHandItem.get();
    auto setFrames = [&](String const& backFrame, String const& frontFrame, bool twoHanded) {
      ToolHand hand = primary || twoHanded ? ToolHand::Primary : ToolHand::Alt;
      humanoid.setHandFrameOverrides(hand, backFrame, frontFrame);
    };
    if (auto swing = as<SwingableItem>(item)) {
      setFrames(swing->getArmFrame(), swing->getArmFrame(), item->twoHanded());
    } else if (as<PointableItem>(item)) {
      setFrames("", "", item->twoHanded());
    } else if (auto active = as<ActiveItem>(item)) {
      setFrames(active->backArmFrame().value_or(String{}), active->frontArmFrame().value_or(String{}), active->twoHandedGrip());
    } else {
      setFrames("", "", false);
    }
  };
  setupHand(true);
  if (!itemSafeTwoHanded(m_primaryHandItem.get()))
    setupHand(false);
}

[[nodiscard]] std::expected<std::optional<Direction>, StarException> ToolUser::setupHumanoidHandItems(Humanoid& humanoid, Vec2F position, Vec2F aimPosition) const {
  setupHumanoidHandItemPose(humanoid);
  if (m_suppress.get() || !m_user) {
    humanoid.setHandParameters(ToolHand::Primary, false, 0.0f, 0.0f, false, false, false);
    humanoid.setHandParameters(ToolHand::Alt, false, 0.0f, 0.0f, false, false, false);
    return {};
  }

  auto inner = [&](bool primary) -> std::expected<std::optional<Direction>, StarException> {
    std::optional<Direction> overrideFacingDirection;

    auto setRotation = [&](bool holdingItem, float angle, float itemAngle, bool twoHanded, bool recoil, bool outsideOfHand) {
      ToolHand hand = primary || twoHanded ? ToolHand::Primary : ToolHand::Alt;
      humanoid.setHandParameters(hand, holdingItem, angle, itemAngle, twoHanded, recoil, outsideOfHand);
    };

    std::shared_ptr<Item> handItem = primary ? m_primaryHandItem.get() : m_altHandItem.get();

    auto world = m_user->get().world();
    if (!world) return std::unexpected(std::move(world.error()));
    auto angleSide = getAngleSide(world->get().geometry().diff(aimPosition, position).angle());

    if (auto swingItem = as<SwingableItem>(handItem)) {
      auto angle = swingItem->getAngleDir(angleSide.first, angleSide.second);
      if (!angle) return std::unexpected(std::move(angle.error()));
      auto itemAngle = swingItem->getItemAngle(angleSide.first);
      if (!itemAngle) return std::unexpected(std::move(itemAngle.error()));
      bool handedness = handItem->twoHanded();
      setRotation(true, *angle, *itemAngle, handedness, false, false);
      overrideFacingDirection = angleSide.second;

    } else if (auto pointableItem = as<PointableItem>(handItem)) {
      auto angle = pointableItem->getAngleDir(angleSide.first, angleSide.second);
      if (!angle) return std::unexpected(std::move(angle.error()));
      setRotation(true, *angle, *angle, handItem->twoHanded(), false, false);
      overrideFacingDirection = angleSide.second;

    } else if (auto activeItem = as<ActiveItem>(handItem)) {
      setRotation(activeItem->holdingItem(), activeItem->armAngle(), activeItem->armAngle(),
          activeItem->twoHandedGrip(), activeItem->recoil(), activeItem->outsideOfHand());
      if (auto fd = activeItem->facingDirection())
        overrideFacingDirection = *fd;

    } else if (auto beamItem = as<BeamItem>(handItem)) {
      auto angle = beamItem->getAngle(angleSide.first);
      if (!angle) return std::unexpected(std::move(angle.error()));
      setRotation(true, *angle, *angle, false, false, false);
      overrideFacingDirection = angleSide.second;

    } else {
      setRotation(false, 0.0f, 0.0f, false, false, false);
    }

    return overrideFacingDirection;
  };

  auto primaryFacing = inner(true);
  if (!primaryFacing) return std::unexpected(std::move(primaryFacing.error()));
  auto overrideFacingDirection = *primaryFacing;
  if (itemSafeTwoHanded(m_primaryHandItem.get())) {
    humanoid.setHandParameters(ToolHand::Alt, false, 0.0f, 0.0f, false, false, false);
  } else {
    auto altFacing = inner(false);
    if (!altFacing) return std::unexpected(std::move(altFacing.error()));
    if (!overrideFacingDirection) overrideFacingDirection = *altFacing;
  }
  return overrideFacingDirection;
}

[[nodiscard]] std::expected<void, StarException> ToolUser::setupHumanoidHandItemDrawables(Humanoid& humanoid, float interpolation) const {
  if (m_suppress.get() || !m_user) {
    humanoid.setHandDrawables(ToolHand::Primary, {});
    humanoid.setHandDrawables(ToolHand::Alt, {});
    humanoid.setHandNonRotatedDrawables(ToolHand::Primary, {});
    humanoid.setHandNonRotatedDrawables(ToolHand::Alt, {});
    return {};
  }

  auto inner = [&](bool primary) -> std::expected<void, StarException> {
    auto setRotated = [&](List<Drawable> drawables, bool twoHanded) {
      if (primary || twoHanded) {
        humanoid.setHandDrawables(ToolHand::Primary, std::move(drawables));
      } else {
        humanoid.setHandDrawables(ToolHand::Alt, std::move(drawables));
      }
    };

    auto setNonRotated = [&](List<Drawable> drawables) {
      if (primary)
        humanoid.setHandNonRotatedDrawables(ToolHand::Primary, std::move(drawables));
      else
        humanoid.setHandNonRotatedDrawables(ToolHand::Alt, std::move(drawables));
    };

    std::shared_ptr<Item> handItem = primary ? m_primaryHandItem.get() : m_altHandItem.get();

    if (auto swingItem = as<SwingableItem>(handItem)) {
      auto drawables = swingItem->drawables();
      if (!drawables) return std::unexpected(std::move(drawables.error()));
      setRotated(std::move(*drawables), handItem->twoHanded());

    } else if (auto pointableItem = as<PointableItem>(handItem)) {
      auto drawables = pointableItem->drawables();
      if (!drawables) return std::unexpected(std::move(drawables.error()));
      setRotated(std::move(*drawables), handItem->twoHanded());

    } else if (auto activeItem = as<ActiveItem>(handItem)) {
      auto drawables = activeItem->handDrawables(interpolation);
      if (!drawables) return std::unexpected(std::move(drawables.error()));
      setRotated(std::move(*drawables), activeItem->twoHandedGrip());

    } else if (auto beamItem = as<BeamItem>(handItem)) {
      auto drawables = beamItem->drawables();
      if (!drawables) return std::unexpected(std::move(drawables.error()));
      setRotated(std::move(*drawables), false);

    } else {
      setRotated({}, false);
    }

    if (auto drawItem = as<NonRotatedDrawablesItem>(handItem)) {
      auto drawables = drawItem->nonRotatedDrawables();
      if (!drawables) return std::unexpected(std::move(drawables.error()));
      setNonRotated(std::move(*drawables));
    } else setNonRotated({});
    return {};
  };

  auto primary = inner(true);
  if (!primary) return std::unexpected(std::move(primary.error()));
  if (itemSafeTwoHanded(m_primaryHandItem.get())) {
    humanoid.setHandDrawables(ToolHand::Alt, {});
    humanoid.setHandNonRotatedDrawables(ToolHand::Alt, {});
  } else {
    auto alt = inner(false);
    if (!alt) return std::unexpected(std::move(alt.error()));
  }
  return {};
}

[[nodiscard]] std::expected<Vec2F, StarException> ToolUser::armPosition(Humanoid const& humanoid, ToolHand hand, Direction facingDirection, float armAngle, Vec2F offset) const {
  if (hand == ToolHand::Primary)
    return humanoid.primaryArmPosition(facingDirection, armAngle, offset);
  else
    return humanoid.altArmPosition(facingDirection, armAngle, offset);
}

[[nodiscard]] std::expected<Vec2F, StarException> ToolUser::handOffset(Humanoid const& humanoid, ToolHand hand, Direction direction) const {
  if (hand == ToolHand::Primary)
    return humanoid.primaryHandOffset(direction);
  else
    return humanoid.altHandOffset(direction);
}

[[nodiscard]] std::expected<Vec2F, StarException> ToolUser::handPosition(ToolHand hand, Humanoid const& humanoid, Vec2F const& handOffset) const {
  if (hand == ToolHand::Primary)
    return humanoid.primaryHandPosition(handOffset);
  else
    return humanoid.altHandPosition(handOffset);
}

[[nodiscard]] std::expected<bool, StarException> ToolUser::queryShieldHit(DamageSource const& source) const {
  if (m_suppress.get() || !m_user) return false;
  for (auto const& item : {m_primaryHandItem.get(), m_altHandItem.get()}) {
    if (auto tool = as<ToolUserItem>(item)) {
      auto polygons = tool->shieldPolys();
      if (!polygons) return std::unexpected(std::move(polygons.error()));
      for (auto poly : *polygons) {
        poly.translate(m_user->get().position());
        auto world = m_user->get().world();
        if (!world) return std::unexpected(std::move(world.error()));
        auto hit = source.intersectsWithPoly(world->get().geometry(), poly);
        if (!hit) return std::unexpected(std::move(hit.error()));
        if (*hit) return true;
      }
    }
  }
  return false;
}

[[nodiscard]] std::expected<void, StarException> ToolUser::tick(float dt, bool shifting, HashSet<MoveControlType> const& moves) {
  if (auto toolUserItem = as<ToolUserItem>(m_primaryHandItem.get())) {
    FireMode fireMode = FireMode::None;
    if (!m_suppress.get()) {
      if (m_fireMain)
        fireMode = FireMode::Primary;
      else if (m_fireAlt && m_primaryHandItem.get()->twoHanded())
        fireMode = FireMode::Alt;
    }
    auto updated = toolUserItem->update(dt, fireMode, shifting, moves);
    if (!updated) return std::unexpected(std::move(updated.error()));
  }

  if (!m_primaryHandItem.get() || !m_primaryHandItem.get()->twoHanded()) {
    if (auto toolUserItem = as<ToolUserItem>(m_altHandItem.get())) {
      FireMode fireMode = FireMode::None;
      if (!m_suppress.get() && m_fireAlt)
        fireMode = FireMode::Primary;
      auto updated = toolUserItem->update(dt, fireMode, shifting, moves);
      if (!updated) return std::unexpected(std::move(updated.error()));
    }
  }

  bool edgeTriggeredMain = m_edgeTriggeredMain;
  m_edgeTriggeredMain = false;
  bool edgeTriggeredAlt = m_edgeTriggeredAlt;
  m_edgeTriggeredAlt = false;

  bool edgeSuppressedMain = m_edgeSuppressedMain;
  m_edgeSuppressedMain = false;
  bool edgeSuppressedAlt = m_edgeSuppressedAlt;
  m_edgeSuppressedAlt = false;

  if (!m_suppress.get()) {
    if (itemSafeTwoHanded(m_primaryHandItem.get()) && (m_fireMain || m_fireAlt)) {
      std::shared_ptr<FireableItem> fireableItem = as<FireableItem>(m_primaryHandItem.get());
      if (fireableItem) {
        FireMode mode;
        if (m_fireMain)
          mode = FireMode::Primary;
        else
          mode = FireMode::Alt;
        auto fired = fireableItem->fire(mode, shifting, edgeTriggeredMain || edgeTriggeredAlt);
        if (!fired) return std::unexpected(std::move(fired.error()));
      }
      std::shared_ptr<ActivatableItem> activatableItem = as<ActivatableItem>(m_primaryHandItem.get());
      if (activatableItem && activatableItem->usable()) {
        auto activated = activatableItem->activate();
        if (!activated) return std::unexpected(std::move(activated.error()));
      }
    } else if (edgeSuppressedMain || (itemSafeTwoHanded(m_primaryHandItem.get()) && edgeSuppressedAlt)) {
      std::shared_ptr<FireableItem> fireableItem = as<FireableItem>(m_primaryHandItem.get());
      if (fireableItem) {
        FireMode mode = FireMode::Primary;
        auto ended = fireableItem->endFire(mode, shifting);
        if (!ended) return std::unexpected(std::move(ended.error()));
      }

      if (m_fireAlt) {
        std::shared_ptr<FireableItem> fireableItem = as<FireableItem>(m_altHandItem.get());
        if (fireableItem) {
          auto fired = fireableItem->fire(FireMode::Alt, shifting, edgeTriggeredAlt);
          if (!fired) return std::unexpected(std::move(fired.error()));
        }
        std::shared_ptr<ActivatableItem> activatableItem = as<ActivatableItem>(m_altHandItem.get());
        if (activatableItem && activatableItem->usable()) {
          auto activated = activatableItem->activate();
          if (!activated) return std::unexpected(std::move(activated.error()));
        }
      }

    } else if (edgeSuppressedAlt) {
      std::shared_ptr<FireableItem> fireableItem = as<FireableItem>(m_altHandItem.get());
      if (fireableItem) {
        FireMode mode = FireMode::Alt;
        auto ended = fireableItem->endFire(mode, shifting);
        if (!ended) return std::unexpected(std::move(ended.error()));
      }

      if (m_fireMain) {
        std::shared_ptr<FireableItem> fireableItem = as<FireableItem>(m_primaryHandItem.get());
        if (fireableItem) {
          auto fired = fireableItem->fire(FireMode::Primary, shifting, edgeTriggeredMain);
          if (!fired) return std::unexpected(std::move(fired.error()));
        }
        std::shared_ptr<ActivatableItem> activatableItem = as<ActivatableItem>(m_primaryHandItem.get());
        if (activatableItem && activatableItem->usable()) {
          auto activated = activatableItem->activate();
          if (!activated) return std::unexpected(std::move(activated.error()));
        }
      }

    } else {
      if (m_fireMain) {
        std::shared_ptr<FireableItem> fireableItem = as<FireableItem>(m_primaryHandItem.get());
        if (fireableItem) {
          auto fired = fireableItem->fire(FireMode::Primary, shifting, edgeTriggeredMain);
          if (!fired) return std::unexpected(std::move(fired.error()));
        }
        std::shared_ptr<ActivatableItem> activatableItem = as<ActivatableItem>(m_primaryHandItem.get());
        if (activatableItem && activatableItem->usable()) {
          auto activated = activatableItem->activate();
          if (!activated) return std::unexpected(std::move(activated.error()));
        }
      }
      if (m_fireAlt) {
        std::shared_ptr<FireableItem> fireableItem = as<FireableItem>(m_altHandItem.get());
        if (fireableItem) {
          auto fired = fireableItem->fire(FireMode::Alt, shifting, edgeTriggeredAlt);
          if (!fired) return std::unexpected(std::move(fired.error()));
        }
        std::shared_ptr<ActivatableItem> activatableItem = as<ActivatableItem>(m_altHandItem.get());
        if (activatableItem && activatableItem->usable()) {
          auto activated = activatableItem->activate();
          if (!activated) return std::unexpected(std::move(activated.error()));
        }
      }
    }
  }
  if (auto beam = as<BeamItem>(m_primaryHandItem.get().get())) {
    auto updated = beam->updateBeamEffects(!m_suppress.get());
    if (!updated) return std::unexpected(std::move(updated.error()));
  }
  if (auto beam = as<BeamItem>(m_altHandItem.get().get())) {
    auto updated = beam->updateBeamEffects(!m_suppress.get() && !itemSafeTwoHanded(m_primaryHandItem.get()));
    if (!updated) return std::unexpected(std::move(updated.error()));
  }
  return {};
}

void ToolUser::beginPrimaryFire() {
  if (!m_fireMain)
    m_edgeTriggeredMain = true;
  m_fireMain = true;
}

void ToolUser::beginAltFire() {
  if (!m_fireAlt)
    m_edgeTriggeredAlt = true;
  m_fireAlt = true;
}

void ToolUser::endPrimaryFire() {
  if (m_fireMain)
    m_edgeSuppressedMain = true;
  m_fireMain = false;
}

void ToolUser::endAltFire() {
  if (m_fireAlt)
    m_edgeSuppressedAlt = true;
  m_fireAlt = false;
}

bool ToolUser::firingPrimary() const {
  return m_fireMain;
}

bool ToolUser::firingAlt() const {
  return m_fireAlt;
}

[[nodiscard]] std::expected<List<DamageSource>, StarException> ToolUser::damageSources() const {
  if (m_suppress.get())
    return {};

  List<DamageSource> ds;
  for (auto const& item : {m_primaryHandItem.get(), m_altHandItem.get()}) {
    if (auto toolItem = as<ToolUserItem>(item)) {
      auto sources = toolItem->damageSources();
      if (!sources) return std::unexpected(std::move(sources.error()));
      ds.appendAll(*sources);
    }
  }
  return ds;
}

[[nodiscard]] std::expected<List<PhysicsForceRegion>, StarException> ToolUser::forceRegions() const {
  if (m_suppress.get())
    return {};

  List<PhysicsForceRegion> ds;
  for (auto const& item : {m_primaryHandItem.get(), m_altHandItem.get()}) {
    if (auto toolItem = as<ToolUserItem>(item)) {
      auto regions = toolItem->forceRegions();
      if (!regions) return std::unexpected(std::move(regions.error()));
      ds.appendAll(*regions);
    }
  }
  return ds;
}

[[nodiscard]] std::expected<void, StarException> ToolUser::collectRenderEvents(RenderCallback& renderCallback) {
  for (auto const& item : {m_primaryHandItem.get(), m_altHandItem.get()}) {
    if (auto active = as<ActiveItem>(item)) {
      auto audio = active->pullNewAudios();
      if (!audio) return std::unexpected(std::move(audio.error()));
      auto particles = active->pullNewParticles();
      if (!particles) return std::unexpected(std::move(particles.error()));
      if (!m_suppress.get()) {
        auto audioAdded = renderCallback.addAudios(std::move(*audio));
        if (!audioAdded) return std::unexpected(std::move(audioAdded.error()));
        auto particlesAdded = renderCallback.addParticles(std::move(*particles));
        if (!particlesAdded) return std::unexpected(std::move(particlesAdded.error()));
      }
    }
  }
  return {};
}

[[nodiscard]] std::expected<void, StarException> ToolUser::render(RenderCallback& renderCallback, bool, bool shifting, EntityRenderLayer renderLayer) {
  if (m_suppress.get())
    return {};
  auto preview = as<PreviewTileTool>(m_primaryHandItem.get());
  if (!preview) preview = as<PreviewTileTool>(m_altHandItem.get());
  if (preview) {
    auto tiles = preview->previewTiles(shifting);
    if (!tiles) return std::unexpected(std::move(tiles.error()));
    auto added = renderCallback.addTilePreviews(*tiles);
    if (!added) return std::unexpected(std::move(added.error()));
  }
  for (auto const& item : {m_primaryHandItem.get(), m_altHandItem.get()}) {
    if (auto renderable = as<RenderableItem>(item)) {
      auto rendered = renderable->render(renderCallback, renderLayer);
      if (!rendered) return std::unexpected(std::move(rendered.error()));
    }
  }
  for (auto const& item : {m_primaryHandItem.get(), m_altHandItem.get()}) {
    if (auto active = as<ActiveItem>(item)) {
      auto drawables = active->entityDrawables();
      if (!drawables) return std::unexpected(std::move(drawables.error()));
      for (auto const& drawable : *drawables) {
        auto added = renderCallback.addDrawable(drawable.first, drawable.second.value_or(renderLayer));
        if (!added) return std::unexpected(std::move(added.error()));
      }
    }
  }
  return {};
}

[[nodiscard]] std::expected<void, StarException> ToolUser::setItems(std::shared_ptr<Item> newPrimaryHandItem, std::shared_ptr<Item> newAltHandItem) {
  if (itemSafeTwoHanded(newPrimaryHandItem))
    newAltHandItem = {};

  if (m_suppress.get()) {
    newPrimaryHandItem = {};
    newAltHandItem = {};
  }

  // Only skip if BOTH items match, to easily handle the edge cases where the
  // primary and alt hands are swapped or share a pointer, to make sure both
  // items end up initialized at the end.
  if (newPrimaryHandItem == m_primaryHandItem.get() && newAltHandItem == m_altHandItem.get())
    return {};

  auto primaryUninit = uninitItem(m_primaryHandItem.get());
  if (!primaryUninit) return std::unexpected(std::move(primaryUninit.error()));
  auto altUninit = uninitItem(m_altHandItem.get());
  if (!altUninit) return std::unexpected(std::move(altUninit.error()));

  // Cancel held fire if we switch primary / alt hand items, to prevent
  // accidentally triggering a switched item without a new edge trigger.

  if (m_primaryHandItem.get() != newPrimaryHandItem) {
    m_fireMain = false;
    m_fireAlt = false;
  }

  if (m_altHandItem.get() != newAltHandItem)
    m_fireAlt = false;

  auto primarySet = m_primaryHandItem.set(std::move(newPrimaryHandItem));
  if (!primarySet) return std::unexpected(std::move(primarySet.error()));
  auto altSet = m_altHandItem.set(std::move(newAltHandItem));
  if (!altSet) return std::unexpected(std::move(altSet.error()));
  auto primaryInit = initPrimaryHandItem();
  if (!primaryInit) return std::unexpected(std::move(primaryInit.error()));
  return initAltHandItem();
}

void ToolUser::suppressItems(bool suppress) {
  m_suppress.set(suppress);
}

[[nodiscard]] std::expected<std::optional<ChainableJsonMessageResponse>, StarException> ToolUser::receiveMessage(String const& message, bool localMessage, JsonArray const& args) {
  for (auto const& item : {m_primaryHandItem.get(), m_altHandItem.get()}) {
    if (auto active = as<ActiveItem>(item)) {
      auto result = active->receiveMessage(message, localMessage, args);
      if (!result) return std::unexpected(std::move(result.error()));
      if (*result) return std::move(*result);
    }
  }
  return std::optional<ChainableJsonMessageResponse>();
}

[[nodiscard]] std::expected<float, StarException> ToolUser::beamGunRadius() const {
  auto bonus = m_user->get().statusController().statusProperty("bonusBeamGunRadius", 0).toFloat();
  if (!bonus) return std::unexpected(std::move(bonus.error()));
  return m_beamGunRadius + *bonus;
}

ToolUser::NetItem::NetItem(Root& root) : m_root(root) {}

void ToolUser::NetItem::initNetVersion(std::optional<std::reference_wrapper<NetElementVersion const>> version) {
  m_netVersion = version;
  m_itemDescriptor.initNetVersion(m_netVersion);
  if (auto netItem = m_item ? asRef<NetElement>(*m_item) : std::nullopt)
    netItem->get().initNetVersion(m_netVersion);
}

[[nodiscard]] std::expected<void, StarException> ToolUser::NetItem::netStore(DataStream& ds, NetCompatibilityRules rules) const {
  if (!checkWithRules(rules)) return {};
  const_cast<NetItem&>(*this).updateItemDescriptor();
  auto descriptor = m_itemDescriptor.netStore(ds, rules);
  if (!descriptor) return std::unexpected(std::move(descriptor.error()));
  if (auto netItem = m_item ? asRef<NetElement>(*m_item) : std::nullopt) return netItem->get().netStore(ds, rules);
  return {};
}

[[nodiscard]] std::expected<void, StarException> ToolUser::NetItem::netLoad(DataStream& ds, NetCompatibilityRules rules) {
  if (!checkWithRules(rules)) return {};
  auto descriptor = m_itemDescriptor.netLoad(ds, rules);
  if (!descriptor) return std::unexpected(std::move(descriptor.error()));
  auto database = m_root.get().itemDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  auto loaded = (*database)->loadItem(m_itemDescriptor.get(), m_item);
  if (!loaded) return std::unexpected(std::move(loaded.error()));
  if (*loaded) {
    m_newItem = true;
    if (auto netItem = m_item ? asRef<NetElement>(*m_item) : std::nullopt) {
      netItem->get().initNetVersion(m_netVersion);
      if (m_netInterpolationEnabled) {
        auto enabled = netItem->get().enableNetInterpolation(m_netExtrapolationHint);
        if (!enabled) return std::unexpected(std::move(enabled.error()));
      }
    }
  }
  if (auto netItem = m_item ? asRef<NetElement>(*m_item) : std::nullopt) return netItem->get().netLoad(ds, rules);
  return {};
}

[[nodiscard]] std::expected<void, StarException> ToolUser::NetItem::enableNetInterpolation(float hint) {
  m_netInterpolationEnabled = true;
  m_netExtrapolationHint = hint;
  if (auto netItem = m_item ? asRef<NetElement>(*m_item) : std::nullopt) return netItem->get().enableNetInterpolation(hint);
  return {};
}

[[nodiscard]] std::expected<void, StarException> ToolUser::NetItem::disableNetInterpolation() {
  m_netInterpolationEnabled = false;
  m_netExtrapolationHint = 0;
  if (auto netItem = m_item ? asRef<NetElement>(*m_item) : std::nullopt) return netItem->get().disableNetInterpolation();
  return {};
}

[[nodiscard]] std::expected<void, StarException> ToolUser::NetItem::tickNetInterpolation(float dt) {
  if (m_netInterpolationEnabled)
    if (auto netItem = m_item ? asRef<NetElement>(*m_item) : std::nullopt) return netItem->get().tickNetInterpolation(dt);
  return {};
}

[[nodiscard]] std::expected<bool, StarException> ToolUser::NetItem::writeNetDelta(DataStream& ds, std::uint64_t fromVersion, NetCompatibilityRules rules) const {
  if (!checkWithRules(rules)) return false;
  bool deltaWritten = false;
  const_cast<NetItem&>(*this).updateItemDescriptor();
  m_buffer.clear();
  auto descriptor = m_itemDescriptor.writeNetDelta(m_buffer, fromVersion, rules);
  if (!descriptor) return std::unexpected(std::move(descriptor.error()));
  if (*descriptor) {
    deltaWritten = true;
    auto code = ds.write<std::uint8_t>(1);
    if (!code) return std::unexpected(std::move(code.error()));
    auto bytes = ds.writeBytes(m_buffer.data());
    if (!bytes) return std::unexpected(std::move(bytes.error()));
    if (auto netItem = m_item ? asRef<NetElement>(*m_item) : std::nullopt) {
      auto code = ds.write<std::uint8_t>(2);
      if (!code) return std::unexpected(std::move(code.error()));
      auto stored = netItem->get().netStore(ds, rules);
      if (!stored) return std::unexpected(std::move(stored.error()));
    }
  }
  if (auto netItem = m_item ? asRef<NetElement>(*m_item) : std::nullopt) {
    m_buffer.clear();
    auto delta = netItem->get().writeNetDelta(m_buffer, fromVersion, rules);
    if (!delta) return std::unexpected(std::move(delta.error()));
    if (*delta) {
      deltaWritten = true;
      auto code = ds.write<std::uint8_t>(3);
      if (!code) return std::unexpected(std::move(code.error()));
      auto bytes = ds.writeBytes(m_buffer.data());
      if (!bytes) return std::unexpected(std::move(bytes.error()));
    }
  }
  if (deltaWritten) {
    auto code = ds.write<std::uint8_t>(0);
    if (!code) return std::unexpected(std::move(code.error()));
  }
  return deltaWritten;
}

[[nodiscard]] std::expected<void, StarException> ToolUser::NetItem::readNetDelta(DataStream& ds, float interpolationTime, NetCompatibilityRules rules) {
  if (!checkWithRules(rules)) return {};
  while (true) {
    auto code = ds.read<std::uint8_t>();
    if (!code) return std::unexpected(std::move(code.error()));
    if (*code == 0) break;
    else if (*code == 1) {
      auto descriptor = m_itemDescriptor.readNetDelta(ds, 0.0f, rules);
      if (!descriptor) return std::unexpected(std::move(descriptor.error()));
      if (!m_item || !m_item->matches(m_itemDescriptor.get(), true)) {
        auto database = m_root.get().itemDatabase();
        if (!database) return std::unexpected(std::move(database.error()));
        auto loaded = (*database)->loadItem(m_itemDescriptor.get(), m_item);
        if (!loaded) return std::unexpected(std::move(loaded.error()));
        if (*loaded) {
          m_newItem = true;
          if (auto netItem = m_item ? asRef<NetElement>(*m_item) : std::nullopt) {
            netItem->get().initNetVersion(m_netVersion);
            if (m_netInterpolationEnabled) {
              auto enabled = netItem->get().enableNetInterpolation(m_netExtrapolationHint);
              if (!enabled) return std::unexpected(std::move(enabled.error()));
            }
          }
        }
      }
    } else if (*code == 2 || *code == 3) {
      auto netItem = m_item ? asRef<NetElement>(*m_item) : std::nullopt;
      if (!netItem) return std::unexpected(IOException("Server/Client disagreement about whether an Item is a NetElement in NetItem::readNetDelta"));
      auto loaded = *code == 2 ? netItem->get().netLoad(ds, rules) : netItem->get().readNetDelta(ds, interpolationTime, rules);
      if (!loaded) return std::unexpected(std::move(loaded.error()));
    } else return std::unexpected(IOException("Improper code received in NetItem::readDelta"));
  }
  return {};
}

[[nodiscard]] std::expected<void, StarException> ToolUser::NetItem::blankNetDelta(float time) {
  if (m_netInterpolationEnabled)
    if (auto netItem = m_item ? asRef<NetElement>(*m_item) : std::nullopt) return netItem->get().blankNetDelta(time);
  return {};
}

std::shared_ptr<Item> const& ToolUser::NetItem::get() const {
  return m_item;
}

[[nodiscard]] std::expected<void, StarException> ToolUser::NetItem::set(std::shared_ptr<Item> item) {
  if (m_item != item) {
    m_item = std::move(item);
    m_newItem = true;
    if (auto netItem = m_item ? asRef<NetElement>(*m_item) : std::nullopt) {
      netItem->get().initNetVersion(m_netVersion);
      auto interpolation = m_netInterpolationEnabled ? netItem->get().enableNetInterpolation(m_netExtrapolationHint) : netItem->get().disableNetInterpolation();
      if (!interpolation) return std::unexpected(std::move(interpolation.error()));
    }
    updateItemDescriptor();
  }
  return {};
}

bool ToolUser::NetItem::pullNewItem() { return std::exchange(m_newItem, false); }

void ToolUser::NetItem::updateItemDescriptor() {
  if (m_item)
    m_itemDescriptor.set(m_item->descriptor());
  else
    m_itemDescriptor.set(ItemDescriptor());
}

[[nodiscard]] std::expected<void, StarException> ToolUser::initPrimaryHandItem() {
  if (m_user && m_primaryHandItem.get()) {
    if (auto tool = as<ToolUserItem>(m_primaryHandItem.get())) {
      auto initialized = tool->init(m_user->get(), ToolHand::Primary);
      if (!initialized) return std::unexpected(std::move(initialized.error()));
    }
    if (auto fireable = as<FireableItem>(m_primaryHandItem.get())) return fireable->triggerCooldown();
  }
  return {};
}

[[nodiscard]] std::expected<void, StarException> ToolUser::initAltHandItem() {
  if (m_altHandItem.get() == m_primaryHandItem.get()) {
    auto cleared = m_altHandItem.set({});
    if (!cleared) return std::unexpected(std::move(cleared.error()));
  }
  if (m_user && m_altHandItem.get()) {
    if (auto tool = as<ToolUserItem>(m_altHandItem.get())) {
      auto initialized = tool->init(m_user->get(), ToolHand::Alt);
      if (!initialized) return std::unexpected(std::move(initialized.error()));
    }
    if (auto fireable = as<FireableItem>(m_altHandItem.get())) return fireable->triggerCooldown();
  }
  return {};
}

[[nodiscard]] std::expected<void, StarException> ToolUser::uninitItem(std::shared_ptr<Item> const& item) {
  if (auto tool = as<ToolUserItem>(item)) return tool->uninit();
  return {};
}

[[nodiscard]] std::expected<void, StarException> ToolUser::netElementsNeedLoad(bool) {
  if (m_primaryHandItem.pullNewItem()) {
    auto initialized = initPrimaryHandItem();
    if (!initialized) return std::unexpected(std::move(initialized.error()));
  }
  if (m_altHandItem.pullNewItem()) {
    auto initialized = initAltHandItem();
    if (!initialized) return std::unexpected(std::move(initialized.error()));
  }

  if (auto fireableItem = as<FireableItem>(m_primaryHandItem.get())) {
    auto fireTime = m_primaryFireTimerNetState.get();
    fireableItem->setCoolingDown(fireTime < 0);
    fireableItem->setFireTimer(std::abs(fireTime));

    auto timeFiring = m_primaryTimeFiringNetState.get();
    fireableItem->setTimeFiring(timeFiring);
  }
  if (auto fireableItem = as<FireableItem>(m_altHandItem.get())) {
    auto fireTime = m_altFireTimerNetState.get();
    fireableItem->setCoolingDown(fireTime < 0);
    fireableItem->setFireTimer(std::abs(fireTime));

    auto timeFiring = m_altTimeFiringNetState.get();
    fireableItem->setTimeFiring(timeFiring);
  }

  if (auto activatableItem = as<ActivatableItem>(m_primaryHandItem.get()))
    activatableItem->setActive(m_primaryItemActiveNetState.get());
  if (auto activatableItem = as<ActivatableItem>(m_altHandItem.get()))
    activatableItem->setActive(m_altItemActiveNetState.get());
  return {};
}

[[nodiscard]] std::expected<void, StarException> ToolUser::netElementsNeedStore() {
  if (auto fireableItem = as<FireableItem>(m_primaryHandItem.get())) {
    auto t = std::max(0.0f, fireableItem->fireTimer());
    if (fireableItem->coolingDown())
      t *= -1;
    m_primaryFireTimerNetState.set(t);
    m_primaryTimeFiringNetState.set(fireableItem->timeFiring());
  } else {
    m_primaryFireTimerNetState.set(0.0);
    m_primaryTimeFiringNetState.set(0.0);
  }
  if (auto fireableItem = as<FireableItem>(m_altHandItem.get())) {
    auto t = std::max(0.0f, fireableItem->fireTimer());
    if (fireableItem->coolingDown())
      t *= -1;
    m_altFireTimerNetState.set(t);
    m_altTimeFiringNetState.set(fireableItem->timeFiring());
  } else {
    m_altFireTimerNetState.set(0.0);
    m_altTimeFiringNetState.set(0.0);
  }

  if (auto activatableItem = as<ActivatableItem>(m_primaryHandItem.get())) {
    auto active = activatableItem->active();
    if (!active) return std::unexpected(std::move(active.error()));
    m_primaryItemActiveNetState.set(*active);
  } else m_primaryItemActiveNetState.set(false);
  if (auto activatableItem = as<ActivatableItem>(m_altHandItem.get())) {
    auto active = activatableItem->active();
    if (!active) return std::unexpected(std::move(active.error()));
    m_altItemActiveNetState.set(*active);
  } else m_altItemActiveNetState.set(false);
  return {};
}

}
