import star.json;
import star.json_extra;
import star.vector;
import star.math_common;
import star.spline;
import star.game_types;
import star.string;
import star.color;
import star.list;
import star.image_processing;
import star.asset_path;
import star.random;
import std;
import star.format;
import star.set;

import star.drawable;
import star.world_geometry;
import star.entity;
import star.particle;
import star.world;
import star.tool_user_entity;
import star.tool_user_item;
import star.beam_item;
import star.assets;
import star.root_base;
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
  auto overrides = config.to_object();
  if (!overrides)
    return std::unexpected(std::move(overrides.error()));
  auto merged = defaults->set_all(*overrides);
  if (!merged)
    return std::unexpected(std::move(merged.error()));
  config = std::move(*merged);
  auto image = config.get_string("image");
  if (!image)
    return std::unexpected(std::move(image.error()));
  m_image = std::move(*image);
  auto endImages = config.get("endImages").and_then(json_to_string_list);
  if (!endImages)
    return std::unexpected(std::move(endImages.error()));
  m_endImages = std::move(*endImages);
  m_endType = EndType::Invalid;
  for (auto const& [key, target] : {
      std::pair{std::string_view("segmentsPerUnit"), std::ref(m_segmentsPerUnit)},
      std::pair{std::string_view("nearControlPointElasticity"), std::ref(m_nearControlPointElasticity)},
      std::pair{std::string_view("farControlPointElasticity"), std::ref(m_farControlPointElasticity)},
      std::pair{std::string_view("nearControlPointDistance"), std::ref(m_nearControlPointDistance)}}) {
    auto value = config.get_float(key);
    if (!value)
      return std::unexpected(std::move(value.error()));
    target.get() = *value;
  }
  auto hand = config.get("handPosition").and_then(json_to_vec2f);
  if (!hand)
    return std::unexpected(std::move(hand.error()));
  m_handPosition = *hand;
  auto fire = config.get("firePosition").and_then(json_to_vec2f);
  if (!fire)
    return std::unexpected(std::move(fire.error()));
  m_firePosition = *fire;
  m_range = 1.0f;
  for (auto const& [key, target] : {
      std::pair{std::string_view("targetSegmentRun"), std::ref(m_targetSegmentRun)},
      std::pair{std::string_view("minBeamWidth"), std::ref(m_minBeamWidth)},
      std::pair{std::string_view("maxBeamWidth"), std::ref(m_maxBeamWidth)}}) {
    auto value = config.get_float(key);
    if (!value)
      return std::unexpected(std::move(value.error()));
    target.get() = *value;
  }
  auto widthDev = config.get_float("beamWidthDev", (m_maxBeamWidth - m_minBeamWidth) / 3);
  if (!widthDev)
    return std::unexpected(std::move(widthDev.error()));
  m_beamWidthDev = *widthDev;
  auto minJitter = config.get_float("minBeamJitter");
  if (!minJitter)
    return std::unexpected(std::move(minJitter.error()));
  m_minBeamJitter = *minJitter;
  auto maxJitter = config.get_float("maxBeamJitter");
  if (!maxJitter)
    return std::unexpected(std::move(maxJitter.error()));
  m_maxBeamJitter = *maxJitter;
  auto jitterDev = config.get_float("beamJitterDev", (m_maxBeamJitter * 2) / 3);
  if (!jitterDev)
    return std::unexpected(std::move(jitterDev.error()));
  m_beamJitterDev = *jitterDev;
  auto minTrans = config.get_float("minBeamTrans");
  if (!minTrans)
    return std::unexpected(std::move(minTrans.error()));
  m_minBeamTrans = *minTrans;
  auto maxTrans = config.get_float("maxBeamTrans");
  if (!maxTrans)
    return std::unexpected(std::move(maxTrans.error()));
  m_maxBeamTrans = *maxTrans;
  auto transDev = config.get_float("beamTransDev", (m_maxBeamTrans - m_minBeamTrans) / 3);
  if (!transDev)
    return std::unexpected(std::move(transDev.error()));
  m_beamTransDev = *transDev;
  auto minLines = config.get_int("minBeamLines");
  if (!minLines)
    return std::unexpected(std::move(minLines.error()));
  m_minBeamLines = *minLines;
  auto maxLines = config.get_int("maxBeamLines");
  if (!maxLines)
    return std::unexpected(std::move(maxLines.error()));
  m_maxBeamLines = *maxLines;
  for (auto const& [key, target] : {
      std::pair{std::string_view("innerBrightnessScale"), std::ref(m_innerBrightnessScale)},
      std::pair{std::string_view("firstStripeThickness"), std::ref(m_firstStripeThickness)},
      std::pair{std::string_view("secondStripeThickness"), std::ref(m_secondStripeThickness)}}) {
    auto value = config.get_float(key);
    if (!value)
      return std::unexpected(std::move(value.error()));
    target.get() = *value;
  }
  m_color = Color::palette_white;
  m_inRangeLastUpdate = false;
  return {};
}

std::expected<void, StarException> BeamItem::init(ToolUserEntity& owner, ToolHand hand) {
  auto initialized = ToolUserItem::init(owner, hand);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  m_beam_curve = {};
  m_beam_arc = {};
  m_target_distance = 0;
  m_inRangeLastUpdate = false;
  m_beamInRange = false;
  m_beamCanPlace = true;
  m_lastUpdateColor = m_color;
  if (!this->initialized())
    return std::unexpected(ItemException("BeamItem::init: Beam Gun not init'd properly, or user not recognized as Tool User."));
  m_color = owner.favorite_color();
  auto radius = owner.beam_gun_radius();
  if (!radius)
    return std::unexpected(std::move(radius.error()));
  m_range = *radius;
  return {};
}

std::expected<void, StarException> BeamItem::update(float, FireMode, bool, HashSet<MoveControlType> const&) {
  if (!initialized())
    return std::unexpected(ItemException("BeamItem::update: Beam Gun not init'd properly, or user not recognized as Tool User."));
  auto item_owner = owner();
  if (!item_owner)
    return std::unexpected(std::move(item_owner.error()));
  auto item_hand = hand();
  if (!item_hand)
    return std::unexpected(std::move(item_hand.error()));
  auto item_world = world();
  if (!item_world)
    return std::unexpected(std::move(item_world.error()));
  auto& user = item_owner->get();
  auto origin = user.hand_position(*item_hand, (m_firePosition - m_handPosition) / tile_pixels);
  if (!origin)
    return std::unexpected(std::move(origin.error()));
  auto aim = user.aim_position();
  if (!aim)
    return std::unexpected(std::move(aim.error()));
  auto const endpoint = m_endType == EndType::TileGroup ? aim->round()
      : m_endType == EndType::Wire ? *aim : center_of_tile(*aim);
  auto const owner_position = user.position();
  auto const dest = item_world->get().geometry().diff(endpoint, owner_position);
  // The separate owner-to-target range check uses the unwrapped world coordinates.
  m_target_distance = (endpoint - owner_position).magnitude();

  Vec2F far_control, near_control;
  if (dest.magnitude_squared() < origin->magnitude_squared()) {
    far_control = dest;
    near_control = *origin;
  } else {
    auto const& far = m_beam_curve[2];
    far_control = far + (dest - far) * m_farControlPointElasticity;
    auto const desired = (dest - *origin) * m_nearControlPointDistance;
    auto const& near = m_beam_curve[1];
    near_control = user.facing_direction() != get_angle_side(near.angle()).second
        ? desired : near + (desired - near) * m_nearControlPointElasticity;
  }

  if (*origin != m_beam_curve.origin() || dest != m_beam_curve.dest()
      || near_control != m_beam_curve[1] || far_control != m_beam_curve[2]) {
    m_beam_curve.origin() = *origin;
    m_beam_curve.dest() = dest;
    m_beam_curve[1] = near_control;
    m_beam_curve[2] = far_control;
    m_beam_curve.sample_arc_lengths(m_beam_arc);
  }
  return {};
}

std::expected<void, StarException> BeamItem::updateBeamEffects(bool visible) {
  m_beamInRange = false;
  if (!visible || !beamActive() || !initialized()) {
    m_inRangeLastUpdate = false;
    return {};
  }

  float const curve_length = m_beam_arc.total_length();
  m_beamInRange = m_target_distance <= m_range && curve_length <= m_range;
  if (m_beamInRange) {
    auto can_place = beam_can_place();
    if (!can_place)
      return std::unexpected(std::move(can_place.error()));
    m_beamCanPlace = *can_place;
    m_lastUpdateColor = m_color;
    if (!m_beamCanPlace)
      m_lastUpdateColor.set_hue(0.0f);
    m_inRangeLastUpdate = true;
    return {};
  }
  if (!std::exchange(m_inRangeLastUpdate, false))
    return {};

  auto item_owner = owner();
  if (!item_owner)
    return std::unexpected(std::move(item_owner.error()));
  auto const owner_position = item_owner->get().position();
  float const segment_run = curve_length * m_targetSegmentRun * 2;
  int const end = static_cast<int>(segment_run - .5);
  float const divisions = static_cast<float>(static_cast<int>(segment_run + .5));
  std::size_t cursor = 0;
  List<Particle> beamLeftovers;
  if (end > 1)
    beamLeftovers.reserve(end - 1);
  for (int i = 1; i < end; ++i) {
    float const t = m_beam_arc.parameter_at_length_fraction(static_cast<float>(i) / divisions, cursor);
    Particle beamParticle;
    beamParticle.type = Particle::Type::Ember;
    beamParticle.position = owner_position + m_beam_curve.point_at(t);
    beamParticle.size = 1.0f;
    Color randomColor = m_lastUpdateColor;
    auto valueScale = beamRoot().random().uniform_float(1, 4);
    if (!valueScale)
      return std::unexpected(std::move(valueScale.error()));
    randomColor.set_value(1 - (1 - randomColor.value()) / *valueScale);
    auto saturationScale = beamRoot().random().uniform_float(1, 4);
    if (!saturationScale)
      return std::unexpected(std::move(saturationScale.error()));
    randomColor.set_saturation(randomColor.saturation() / *saturationScale);
    beamParticle.color = randomColor;
    beamParticle.velocity = Vec2F::filled(beamRoot().random().uniform_float());
    beamParticle.final_velocity = Vec2F(0.0f, -20.0f);
    beamParticle.approach = Vec2F(0.0f, 5.0f);
    beamParticle.time_to_live = 0.25f;
    beamParticle.destruction_action = Particle::DestructionAction::Shrink;
    beamParticle.destruction_time = 0.2f;
    beamLeftovers.push_back(std::move(beamParticle));
  }
  // Capture world positions at the producing tick, not at display sampling or
  // a later owner position.
  return item_owner->get().add_particles(beamLeftovers);
}

bool BeamItem::beamActive() const {
  return true;
}

std::expected<bool, StarException> BeamItem::beam_can_place() const {
  return true;
}

std::expected<List<Drawable>, StarException> BeamItem::nonRotatedDrawables() const {
  return beamDrawables();
}

std::expected<float, StarException> BeamItem::getAngle(float angle) {
  if (m_beam_curve.dest().magnitude_squared() < m_beam_curve.origin().magnitude_squared()
      || m_beam_curve.origin() == m_beam_curve[1])
    return angle;
  return get_angle_side(m_beam_curve[1].angle()).first;
}

std::expected<List<Drawable>, StarException> BeamItem::drawables() const {
  auto metadata = beamRoot().image_metadata_database();
  if (!metadata)
    return std::unexpected(std::move(metadata.error()));
  auto drawable = Drawable::make_image(**metadata, m_image, 1.0f / tile_pixels, true, -handPosition() / tile_pixels);
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
  const float rangeEffect = (m_range - m_beam_arc.total_length()) / m_range;

  auto projectOntoRange = [&](float min, float max) { return rangeEffect * (max - min) + min; };
  auto rangeRand = [&](float dev, float min, float max) -> std::expected<float, StarException> {
    auto value = beamRoot().presentationRandom().normal_float(dev, projectOntoRange(min, max));
    if (!value)
      return std::unexpected(std::move(value.error()));
    return clamp<float>(*value, min, max);
  };

  int numLines = projectOntoRange(m_minBeamLines, m_maxBeamLines);
  auto transparency = rangeRand(m_beamTransDev, m_minBeamTrans, m_maxBeamTrans);
  if (!transparency)
    return std::unexpected(std::move(transparency.error()));
  float res = 1 - *transparency;
  if (numLines > 0) {
    for (auto line = 0; line < numLines - 1; line++) {
      auto nextTransparency = rangeRand(m_beamTransDev, m_minBeamTrans, m_maxBeamTrans);
      if (!nextTransparency)
        return std::unexpected(std::move(nextTransparency.error()));
      res *= 1 - *nextTransparency;
    }
  }
  return 1 - res;
}

void BeamItem::setEnd(EndType type) {
  m_endType = type;
}

std::expected<List<Drawable>, StarException> BeamItem::beamDrawables() const {
  List<Drawable> res;

  const float curve_length = m_beam_arc.total_length();
  const float rangeEffect = (m_range - curve_length) / m_range;

  auto projectOntoRange = [&](float min, float max) { return rangeEffect * (max - min) + min; };
  auto rangeRand = [&](float dev, float min, float max) -> std::expected<float, StarException> {
    auto value = beamRoot().presentationRandom().normal_float(dev, projectOntoRange(min, max));
    if (!value)
      return std::unexpected(std::move(value.error()));
    return clamp<float>(*value, min, max);
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
        auto metadata = beamRoot().image_metadata_database();
        if (!metadata)
          return std::unexpected(std::move(metadata.error()));
        auto ball = Drawable::make_image(**metadata, endImage, 1.0f / tile_pixels, true, m_beam_curve.dest());
        if (!ball)
          return std::unexpected(std::move(ball.error()));
        Color ballColor = Color::palette_white;
        auto opacity = getAppropriateOpacity();
        if (!opacity)
          return std::unexpected(std::move(opacity.error()));
        ballColor.set_alpha_f(*opacity);
        ball->color = ballColor;
        res.push_back(std::move(*ball));
      }

      float const segment_run = curve_length * m_targetSegmentRun;
      int const end = static_cast<int>(segment_run - .5);
      float const divisions = static_cast<float>(static_cast<int>(segment_run + .5));
      for (auto line = 0; line < numLines; line++) {
        auto width = rangeRand(m_beamWidthDev, m_minBeamWidth, m_maxBeamWidth);
        if (!width)
          return std::unexpected(std::move(width.error()));
        float lineThickness = *width;
        auto transparency = rangeRand(m_beamTransDev, m_minBeamTrans, m_maxBeamTrans);
        if (!transparency)
          return std::unexpected(std::move(transparency.error()));
        float beamTransparency = *transparency;
        mainColor.set_alpha_f(mainColor.alpha_f() * beamTransparency);
        Vec2F previousLoc = m_beam_curve.origin(); // lines meet at origin and dest.
        Color innerStripe = mainColor;
        innerStripe.set_value(1 - (1 - innerStripe.value()) / m_innerBrightnessScale);
        innerStripe.set_saturation(innerStripe.saturation() / m_innerBrightnessScale);
        Color const firstStripe = Color::rgba(innerStripe.to_rgba());
        innerStripe.set_value(1 - (1 - innerStripe.value()) / m_innerBrightnessScale);
        innerStripe.set_saturation(innerStripe.saturation() / m_innerBrightnessScale);
        Color const secondStripe = Color::rgba(innerStripe.to_rgba());

        for (int i = 1; i < end; ++i) {
          // The visible stripes are uniform in parameter, not in arc length.
          float const pos = static_cast<float>(i) / divisions;

          auto jitterX = rangeRand(m_beamJitterDev, -m_maxBeamJitter, m_maxBeamJitter);
          if (!jitterX)
            return std::unexpected(std::move(jitterX.error()));
          auto jitterY = rangeRand(m_beamJitterDev, -m_maxBeamJitter, m_maxBeamJitter);
          if (!jitterY)
            return std::unexpected(std::move(jitterY.error()));
          Vec2F currentLoc = m_beam_curve.point_at(pos) + Vec2F(*jitterX, *jitterY);
          res.push_back(
              Drawable::make_line(Line2F(previousLoc, currentLoc), lineThickness, mainColor, Vec2F()));
          res.push_back(Drawable::make_line(Line2F(previousLoc, currentLoc),
              lineThickness * m_firstStripeThickness,
              firstStripe,
              Vec2F()));
          res.push_back(Drawable::make_line(Line2F(previousLoc, currentLoc),
              lineThickness * m_secondStripeThickness,
              secondStripe,
              Vec2F()));
          previousLoc = std::move(currentLoc);
        }
        res.push_back(Drawable::make_line(
            Line2F(previousLoc, m_beam_curve.dest()), lineThickness, mainColor, Vec2F()));
        res.push_back(Drawable::make_line(Line2F(previousLoc, m_beam_curve.dest()),
            lineThickness * m_firstStripeThickness,
            firstStripe,
            Vec2F()));
        res.push_back(Drawable::make_line(Line2F(previousLoc, m_beam_curve.dest()),
            lineThickness * m_secondStripeThickness,
            secondStripe,
            Vec2F()));
      }
  }
  return res;
}

Root& BeamItem::beamRoot() const {
  return m_beamRoot.get();
}

}
