import star.json;
import star.json_extra;
import star.id_map;
import star.vector;
import star.math_common;
import star.spline;
import star.game_types;
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
import star.strong_typedef;
import star.bi_map;
import star.rpc_promise;
import star.a_star;
import star.random;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.format;
import star.version;
import star.set;

import star.drawable;
import star.non_rotated_drawables_item;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.animation;
import star.particle;
import star.interaction_types;
import star.tile_damage;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_modification;
import star.lua_root;
import star.force_regions;
import star.world;
import star.physics_entity;
import star.movement_controller;
import star.platformer_astar_types;
import star.anchorable_entity;
import star.game_timers;
import star.actor_movement_controller;
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.tool_user_item;
import star.beam_item;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.item;

import star.line;
namespace Star {

BeamItem::BeamItem(Root& root) : m_beamRoot(root) {}

std::expected<void, StarException> BeamItem::initializeBeam(Json config) {
  auto assets = beamRoot().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto defaults = (*assets)->json("/player.config:beamGunConfig");
  if (!defaults)
    return std::unexpected(std::move(defaults.error()));
  auto overrides = config.toObject();
  if (!overrides)
    return std::unexpected(std::move(overrides.error()));
  auto merged = defaults->setAll(*overrides);
  if (!merged)
    return std::unexpected(std::move(merged.error()));
  config = std::move(*merged);
  auto image = config.getString("image");
  if (!image)
    return std::unexpected(std::move(image.error()));
  m_image = std::move(*image);
  auto endImages = config.get("endImages").and_then(jsonToStringList);
  if (!endImages)
    return std::unexpected(std::move(endImages.error()));
  m_endImages = std::move(*endImages);
  m_endType = EndType::Invalid;
  for (auto const& [key, target] : {
      std::pair{std::string_view("segmentsPerUnit"), std::ref(m_segmentsPerUnit)},
      std::pair{std::string_view("nearControlPointElasticity"), std::ref(m_nearControlPointElasticity)},
      std::pair{std::string_view("farControlPointElasticity"), std::ref(m_farControlPointElasticity)},
      std::pair{std::string_view("nearControlPointDistance"), std::ref(m_nearControlPointDistance)}}) {
    auto value = config.getFloat(key);
    if (!value)
      return std::unexpected(std::move(value.error()));
    target.get() = *value;
  }
  auto hand = config.get("handPosition").and_then(jsonToVec2F);
  if (!hand)
    return std::unexpected(std::move(hand.error()));
  m_handPosition = *hand;
  auto fire = config.get("firePosition").and_then(jsonToVec2F);
  if (!fire)
    return std::unexpected(std::move(fire.error()));
  m_firePosition = *fire;
  m_range = 1.0f;
  for (auto const& [key, target] : {
      std::pair{std::string_view("targetSegmentRun"), std::ref(m_targetSegmentRun)},
      std::pair{std::string_view("minBeamWidth"), std::ref(m_minBeamWidth)},
      std::pair{std::string_view("maxBeamWidth"), std::ref(m_maxBeamWidth)}}) {
    auto value = config.getFloat(key);
    if (!value)
      return std::unexpected(std::move(value.error()));
    target.get() = *value;
  }
  auto widthDev = config.getFloat("beamWidthDev", (m_maxBeamWidth - m_minBeamWidth) / 3);
  if (!widthDev)
    return std::unexpected(std::move(widthDev.error()));
  m_beamWidthDev = *widthDev;
  auto minJitter = config.getFloat("minBeamJitter");
  if (!minJitter)
    return std::unexpected(std::move(minJitter.error()));
  m_minBeamJitter = *minJitter;
  auto maxJitter = config.getFloat("maxBeamJitter");
  if (!maxJitter)
    return std::unexpected(std::move(maxJitter.error()));
  m_maxBeamJitter = *maxJitter;
  auto jitterDev = config.getFloat("beamJitterDev", (m_maxBeamJitter * 2) / 3);
  if (!jitterDev)
    return std::unexpected(std::move(jitterDev.error()));
  m_beamJitterDev = *jitterDev;
  auto minTrans = config.getFloat("minBeamTrans");
  if (!minTrans)
    return std::unexpected(std::move(minTrans.error()));
  m_minBeamTrans = *minTrans;
  auto maxTrans = config.getFloat("maxBeamTrans");
  if (!maxTrans)
    return std::unexpected(std::move(maxTrans.error()));
  m_maxBeamTrans = *maxTrans;
  auto transDev = config.getFloat("beamTransDev", (m_maxBeamTrans - m_minBeamTrans) / 3);
  if (!transDev)
    return std::unexpected(std::move(transDev.error()));
  m_beamTransDev = *transDev;
  auto minLines = config.getInt("minBeamLines");
  if (!minLines)
    return std::unexpected(std::move(minLines.error()));
  m_minBeamLines = *minLines;
  auto maxLines = config.getInt("maxBeamLines");
  if (!maxLines)
    return std::unexpected(std::move(maxLines.error()));
  m_maxBeamLines = *maxLines;
  for (auto const& [key, target] : {
      std::pair{std::string_view("innerBrightnessScale"), std::ref(m_innerBrightnessScale)},
      std::pair{std::string_view("firstStripeThickness"), std::ref(m_firstStripeThickness)},
      std::pair{std::string_view("secondStripeThickness"), std::ref(m_secondStripeThickness)}}) {
    auto value = config.getFloat(key);
    if (!value)
      return std::unexpected(std::move(value.error()));
    target.get() = *value;
  }
  m_color = Color::White;
  m_inRangeLastUpdate = false;
  return {};
}

std::expected<void, StarException> BeamItem::init(ToolUserEntity& owner, ToolHand hand) {
  auto initialized = ToolUserItem::init(owner, hand);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  m_beamCurve = CSplineF();
  m_inRangeLastUpdate = false;
  m_beamInRange = false;
  m_beamCanPlace = true;
  m_lastUpdateColor = m_color;
  if (!this->initialized())
    return std::unexpected(ItemException("BeamItem::init: Beam Gun not init'd properly, or user not recognized as Tool User."));
  m_color = owner.favoriteColor();
  auto radius = owner.beamGunRadius();
  if (!radius)
    return std::unexpected(std::move(radius.error()));
  m_range = *radius;
  return {};
}

std::expected<void, StarException> BeamItem::update(float, FireMode, bool, HashSet<MoveControlType> const&) {
  if (!initialized())
    return std::unexpected(ItemException("BeamItem::update: Beam Gun not init'd properly, or user not recognized as Tool User."));
  auto itemOwner = owner();
  if (!itemOwner)
    return std::unexpected(std::move(itemOwner.error()));
  auto itemHand = hand();
  if (!itemHand)
    return std::unexpected(std::move(itemHand.error()));
  auto itemWorld = world();
  if (!itemWorld)
    return std::unexpected(std::move(itemWorld.error()));
  auto position = itemOwner->get().handPosition(*itemHand, (m_firePosition - m_handPosition) / TilePixels);
  if (!position)
    return std::unexpected(std::move(position.error()));
  m_beamCurve.origin() = *position;
  auto aimPosition = itemOwner->get().aimPosition();
  if (!aimPosition)
    return std::unexpected(std::move(aimPosition.error()));
  if (m_endType == EndType::TileGroup)
    m_beamCurve.dest() = itemWorld->get().geometry().diff(aimPosition->round(), itemOwner->get().position());
  else if (m_endType == EndType::Wire)
    m_beamCurve.dest() = itemWorld->get().geometry().diff(*aimPosition, itemOwner->get().position());
  else
    m_beamCurve.dest() = itemWorld->get().geometry().diff(centerOfTile(*aimPosition), itemOwner->get().position());

  if (m_beamCurve.dest().magnitude_squared() < m_beamCurve.origin().magnitude_squared())
    m_beamCurve[2] = m_beamCurve.dest();
  else
    m_beamCurve[2] = m_beamCurve[2] + (m_beamCurve.dest() - m_beamCurve[2]) * m_farControlPointElasticity;

  Vec2F desiredNearControlPoint = (m_beamCurve.dest() - m_beamCurve.origin()) * m_nearControlPointDistance;

  if (m_beamCurve.dest().magnitude_squared() < m_beamCurve.origin().magnitude_squared())
    m_beamCurve[1] = m_beamCurve.origin();
  else if (itemOwner->get().facingDirection() != getAngleSide(m_beamCurve[1].angle()).second)
    m_beamCurve[1] = desiredNearControlPoint;
  else
    m_beamCurve[1] = m_beamCurve[1] + (desiredNearControlPoint - m_beamCurve[1]) * m_nearControlPointElasticity;
  return {};
}

std::expected<void, StarException> BeamItem::updateBeamEffects(bool visible) {
  m_beamInRange = false;
  if (!visible || !beamActive() || !initialized()) {
    m_inRangeLastUpdate = false;
    return {};
  }

  auto itemOwner = owner();
  if (!itemOwner)
    return std::unexpected(std::move(itemOwner.error()));
  auto aimPosition = itemOwner->get().aimPosition();
  if (!aimPosition)
    return std::unexpected(std::move(aimPosition.error()));
  Vec2F endPoint;
  if (m_endType == EndType::TileGroup)
    endPoint = aimPosition->round();
  else if (m_endType == EndType::Wire)
    endPoint = *aimPosition;
  else
    endPoint = centerOfTile(*aimPosition);

  float curveLen = m_beamCurve.length();
  m_beamInRange = (endPoint - itemOwner->get().position()).magnitude() <= m_range && curveLen <= m_range;
  if (m_beamInRange) {
    auto canPlace = beamCanPlace();
    if (!canPlace)
      return std::unexpected(std::move(canPlace.error()));
    m_beamCanPlace = *canPlace;
    m_lastUpdateColor = m_color;
    if (!m_beamCanPlace)
      m_lastUpdateColor.setHue(m_lastUpdateColor.hue() + 120);
    m_inRangeLastUpdate = true;
    return {};
  }
  if (!std::exchange(m_inRangeLastUpdate, false))
    return {};

  List<Particle> beamLeftovers;
  for (auto i = 1; i < static_cast<int>(curveLen * m_targetSegmentRun * 2 - .5); i++) {
    float pos = static_cast<float>(i) / static_cast<float>(static_cast<int>(curveLen * m_targetSegmentRun * 2 + .5));
    float curveLoc = m_beamCurve.arcLenPara(pos);
    Particle beamParticle;
    beamParticle.type = Particle::Type::Ember;
    beamParticle.position = itemOwner->get().position() + m_beamCurve.pointAt(curveLoc);
    beamParticle.size = 1.0f;
    Color randomColor = m_lastUpdateColor;
    auto valueScale = beamRoot().random().randf(1, 4);
    if (!valueScale)
      return std::unexpected(std::move(valueScale.error()));
    randomColor.setValue(1 - (1 - randomColor.value()) / *valueScale);
    auto saturationScale = beamRoot().random().randf(1, 4);
    if (!saturationScale)
      return std::unexpected(std::move(saturationScale.error()));
    randomColor.setSaturation(randomColor.saturation() / *saturationScale);
    beamParticle.color = randomColor;
    beamParticle.velocity = Vec2F::filled(beamRoot().random().randf());
    beamParticle.finalVelocity = Vec2F(0.0f, -20.0f);
    beamParticle.approach = Vec2F(0.0f, 5.0f);
    beamParticle.timeToLive = 0.25f;
    beamParticle.destructionAction = Particle::DestructionAction::Shrink;
    beamParticle.destructionTime = 0.2f;
    beamLeftovers.push_back(std::move(beamParticle));
  }
  // Capture world positions at the producing tick, not at display sampling or
  // a later owner position.
  return itemOwner->get().addParticles(beamLeftovers);
}

bool BeamItem::beamActive() const {
  return true;
}

std::expected<bool, StarException> BeamItem::beamCanPlace() const {
  return true;
}

std::expected<List<Drawable>, StarException> BeamItem::nonRotatedDrawables() const {
  return beamDrawables();
}

std::expected<float, StarException> BeamItem::getAngle(float angle) {
  if (m_beamCurve.dest().magnitude_squared() < m_beamCurve.origin().magnitude_squared()
      || m_beamCurve.origin() == m_beamCurve[1])
    return angle;
  return getAngleSide(m_beamCurve[1].angle()).first;
}

std::expected<List<Drawable>, StarException> BeamItem::drawables() const {
  auto metadata = beamRoot().imageMetadataDatabase();
  if (!metadata)
    return std::unexpected(std::move(metadata.error()));
  auto drawable = Drawable::makeImage(**metadata, m_image, 1.0f / TilePixels, true, -handPosition() / TilePixels);
  if (!drawable)
    return std::unexpected(std::move(drawable.error()));
  return List<Drawable>{std::move(*drawable)};
}

Vec2F BeamItem::handPosition() const {
  return m_handPosition;
}

Vec2F BeamItem::firePosition() const {
  return m_firePosition;
}

void BeamItem::setRange(float range) {
  m_range = range;
}

std::expected<float, StarException> BeamItem::getAppropriateOpacity() const {
  float curveLen = m_beamCurve.length();
  const float rangeEffect = (m_range - curveLen) / m_range;

  auto projectOntoRange = [&](float min, float max) { return rangeEffect * (max - min) + min; };
  auto rangeRand = [&](float dev, float min, float max) {
    return clamp<float>(beamRoot().presentationRandom().nrandf(dev, projectOntoRange(min, max)), min, max);
  };

  int numLines = projectOntoRange(m_minBeamLines, m_maxBeamLines);
  float res = (1 - rangeRand(m_beamTransDev, m_minBeamTrans, m_maxBeamTrans));
  if (numLines > 0) {
    for (auto line = 0; line < numLines - 1; line++)
      res *= (1 - rangeRand(m_beamTransDev, m_minBeamTrans, m_maxBeamTrans));
  }
  return 1 - res;
}

void BeamItem::setEnd(EndType type) {
  m_endType = type;
}

std::expected<List<Drawable>, StarException> BeamItem::beamDrawables() const {
  List<Drawable> res;

  float curveLen = m_beamCurve.length();
  const float rangeEffect = (m_range - curveLen) / m_range;

  auto projectOntoRange = [&](float min, float max) { return rangeEffect * (max - min) + min; };
  auto rangeRand = [&](float dev, float min, float max) {
    return clamp<float>(beamRoot().presentationRandom().nrandf(dev, projectOntoRange(min, max)), min, max);
  };

  if (initialized() && m_beamInRange) {
      int numLines = projectOntoRange(m_minBeamLines, m_maxBeamLines);
      Color mainColor = m_lastUpdateColor;

      String endImage = "";
      if (m_endType != EndType::Invalid) {
        endImage = m_endImages[static_cast<unsigned>(m_endType)];
      }

      if (!endImage.empty()) {
        if (!m_beamCanPlace) {
          ImageOperation op = HueShiftImageOperation::hue_shift_degrees(120);
          endImage = Star::format("{}?{}", endImage, image_operation_to_string(op));
        }
        auto metadata = beamRoot().imageMetadataDatabase();
        if (!metadata)
          return std::unexpected(std::move(metadata.error()));
        auto ball = Drawable::makeImage(**metadata, endImage, 1.0f / TilePixels, true, m_beamCurve.dest());
        if (!ball)
          return std::unexpected(std::move(ball.error()));
        Color ballColor = Color::White;
        auto opacity = getAppropriateOpacity();
        if (!opacity)
          return std::unexpected(std::move(opacity.error()));
        ballColor.setAlphaF(*opacity);
        ball->color = ballColor;
        res.push_back(std::move(*ball));
      }

      for (auto line = 0; line < numLines; line++) {
        float lineThickness = rangeRand(m_beamWidthDev, m_minBeamWidth, m_maxBeamWidth);
        float beamTransparency = rangeRand(m_beamTransDev, m_minBeamTrans, m_maxBeamTrans);
        mainColor.setAlphaF(mainColor.alphaF() * beamTransparency);
        Vec2F previousLoc = m_beamCurve.origin(); // lines meet at origin and dest.
        Color innerStripe = mainColor;
        innerStripe.setValue(1 - (1 - innerStripe.value()) / m_innerBrightnessScale);
        innerStripe.setSaturation(innerStripe.saturation() / m_innerBrightnessScale);
        Vec4B firstStripe = innerStripe.toRgba();
        innerStripe.setValue(1 - (1 - innerStripe.value()) / m_innerBrightnessScale);
        innerStripe.setSaturation(innerStripe.saturation() / m_innerBrightnessScale);
        Vec4B secondStripe = innerStripe.toRgba();

        for (auto i = 1; i < static_cast<int>(curveLen * m_targetSegmentRun - .5); i++) {// one less than full length
          float pos = static_cast<float>(i)
            / static_cast<float>(
                        static_cast<int>(curveLen * m_targetSegmentRun + .5));// project the discrete steps evenly

          Vec2F currentLoc =
              m_beamCurve.pointAt(pos) + Vec2F(rangeRand(m_beamJitterDev, -m_maxBeamJitter, m_maxBeamJitter),
                                             rangeRand(m_beamJitterDev, -m_maxBeamJitter, m_maxBeamJitter));
          res.push_back(
              Drawable::makeLine(Line2F(previousLoc, currentLoc), lineThickness, mainColor, Vec2F()));
          res.push_back(Drawable::makeLine(Line2F(previousLoc, currentLoc),
              lineThickness * m_firstStripeThickness,
              Color::rgba(firstStripe),
              Vec2F()));
          res.push_back(Drawable::makeLine(Line2F(previousLoc, currentLoc),
              lineThickness * m_secondStripeThickness,
              Color::rgba(secondStripe),
              Vec2F()));
          previousLoc = std::move(currentLoc);
        }
        res.push_back(Drawable::makeLine(
            Line2F(previousLoc, m_beamCurve.dest()), lineThickness, mainColor, Vec2F()));
        res.push_back(Drawable::makeLine(Line2F(previousLoc, m_beamCurve.dest()),
            lineThickness * m_firstStripeThickness,
            Color::rgba(firstStripe),
            Vec2F()));
        res.push_back(Drawable::makeLine(Line2F(previousLoc, m_beamCurve.dest()),
            lineThickness * m_secondStripeThickness,
            Color::rgba(secondStripe),
            Vec2F()));
      }
  }
  return res;
}

Root& BeamItem::beamRoot() const {
  return m_beamRoot.get();
}

}
