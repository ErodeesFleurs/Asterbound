import star.memory;
import star.json;
import star.json_extra;
import star.data_stream;
import star.game_types;
import star.string;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.bi_map;
import star.random;
import star.periodic_function;
import star.ordered_map;
import star.matrix3;
import star.interpolation;
import star.vector;
import star.audio;
import star.map;
import star.set;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.optional;
import star.format;
import star.version;
import star.data_stream_extra;
import star.id_map;
import star.strong_typedef;
import star.casting;
import star.rpc_promise;
import star.a_star;

import star.drawable;
import star.animation;
import star.particle;
import star.animated_part_set;
import star.mixer;
import star.light_source;
import star.networked_animator;
import star.humanoid;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.item;
import star.status_types;
import star.status_effect_item;
import star.effect_source_item;
import star.previewable_item;
import star.damage_types;
import star.world_geometry;
import star.damage;
import star.entity;
import star.interaction_types;
import star.tile_damage;
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
import star.lua_components;
import star.fireable_item;
import star.swingable_item;
import star.armors;
import star.species_database;
import star.dance_database;
import star.particle_database;

import star.net_element_sync_group;

import star.net_compatibility;
import star.math_common;
import star.algorithm;
namespace Star {

extern std::expected<EnumMap<HumanoidEmote>, StarException> const HumanoidEmoteNames = EnumMap<HumanoidEmote>::create({
  {HumanoidEmote::Idle, "Idle"},
  {HumanoidEmote::Blabbering, "Blabbering"},
  {HumanoidEmote::Shouting, "Shouting"},
  {HumanoidEmote::Happy, "Happy"},
  {HumanoidEmote::Sad, "Sad"},
  {HumanoidEmote::NEUTRAL, "NEUTRAL"},
  {HumanoidEmote::Laugh, "Laugh"},
  {HumanoidEmote::Annoyed, "Annoyed"},
  {HumanoidEmote::Oh, "Oh"},
  {HumanoidEmote::OOOH, "OOOH"},
  {HumanoidEmote::Blink, "Blink"},
  {HumanoidEmote::Wink, "Wink"},
  {HumanoidEmote::Eat, "Eat"},
  {HumanoidEmote::Sleep, "Sleep"}
});

// Raptor - I hate the fact that its inconsistent between the strings used in the map above and with the switch case down below
extern std::expected<EnumMap<HumanoidEmote>, StarException> const HumanoidEmoteFrameBaseNames = EnumMap<HumanoidEmote>::create({
  {HumanoidEmote::Idle, "idle"},
  {HumanoidEmote::Blabbering, "blabber"},
  {HumanoidEmote::Shouting, "shout"},
  {HumanoidEmote::Happy, "happy"},
  {HumanoidEmote::Sad, "sad"},
  {HumanoidEmote::NEUTRAL, "neutral"},
  {HumanoidEmote::Laugh, "laugh"},
  {HumanoidEmote::Annoyed, "annoyed"},
  {HumanoidEmote::Oh, "oh"},
  {HumanoidEmote::OOOH, "oooh"},
  {HumanoidEmote::Blink, "blink"},
  {HumanoidEmote::Wink, "wink"},
  {HumanoidEmote::Eat, "eat"},
  {HumanoidEmote::Sleep, "sleep"}
});

std::expected<Personality, StarException> parsePersonalityArray(Json const& config) {
  {
auto checked1 = config.getString(0);
if (!checked1) return std::unexpected(std::move(checked1.error()));
auto checked2 = config.getString(1);
if (!checked2) return std::unexpected(std::move(checked2.error()));
auto checked3 = config.get(2);
if (!checked3) return std::unexpected(std::move(checked3.error()));
auto checked4 = jsonToVec2F(std::move(*checked3));
if (!checked4) return std::unexpected(std::move(checked4.error()));
auto checked5 = config.get(3);
if (!checked5) return std::unexpected(std::move(checked5.error()));
auto checked6 = jsonToVec2F(std::move(*checked5));
if (!checked6) return std::unexpected(std::move(checked6.error()));
return Personality{std::move(*checked1), std::move(*checked2), std::move(*checked4), std::move(*checked6)};
}
}

std::expected<std::reference_wrapper<Personality>, StarException> parsePersonality(Personality& personality, Json const& config) {
  {
auto checked7 = config.opt("idle");
if (!checked7) return std::unexpected(std::move(checked7.error()));
if (auto idle = std::move(*checked7))
    {
      auto idleValue = idle->toString();
      if (!idleValue) return std::unexpected(std::move(idleValue.error()));
      personality.idle = std::move(*idleValue);
    }
}
  {
auto checked8 = config.opt("armIdle");
if (!checked8) return std::unexpected(std::move(checked8.error()));
if (auto armIdle = std::move(*checked8))
    {
      auto armIdleValue = armIdle->toString();
      if (!armIdleValue) return std::unexpected(std::move(armIdleValue.error()));
      personality.armIdle = std::move(*armIdleValue);
    }
}
  {
auto checked9 = config.opt("headOffset");
if (!checked9) return std::unexpected(std::move(checked9.error()));
if (auto headOffset = std::move(*checked9))
    {
auto checked10 = jsonToVec2F(*headOffset);
if (!checked10) return std::unexpected(std::move(checked10.error()));
personality.headOffset = std::move(*checked10);
}
}
  {
auto checked11 = config.opt("armOffset");
if (!checked11) return std::unexpected(std::move(checked11.error()));
if (auto armOffset = std::move(*checked11))
    {
auto checked12 = jsonToVec2F(*armOffset);
if (!checked12) return std::unexpected(std::move(checked12.error()));
personality.armOffset = std::move(*checked12);
}
}

  return std::ref(personality);
}

std::expected<Personality, StarException> parsePersonality(Json const& config) {
  Personality personality;
  auto parsed = parsePersonality(personality, config);
  if (!parsed) return std::unexpected(std::move(parsed.error()));
  return personality;
}

Json jsonFromPersonality(Personality const& personality) {
  return JsonObject{
    { "idle", personality.idle },
    { "armIdle", personality.armIdle },
    { "headOffset", jsonFromVec2F(personality.headOffset) },
    { "armOffset", jsonFromVec2F(personality.armOffset)   }
  };
}

HumanoidIdentity::HumanoidIdentity()
  : name("Humanoid"), species("human"), gender(Gender::Male), hairGroup("hair"), hairType("male1"), color(51, 117, 237, 255) {}

std::expected<HumanoidIdentity, StarException> HumanoidIdentity::create(Json config) {
  HumanoidIdentity identity;
  if (config.isNull())
    config = JsonObject();

  {
auto checked13 = config.getString("name", "Humanoid");
if (!checked13) return std::unexpected(std::move(checked13.error()));
identity.name = std::move(*checked13);
}
  {
auto checked14 = config.getString("species", "human");
if (!checked14) return std::unexpected(std::move(checked14.error()));
identity.species = std::move(*checked14);
}
  {
auto checked15 = config.getString("gender", "male");
if (!checked15) return std::unexpected(std::move(checked15.error()));
auto checked16 = enumValue<GenderNames>(std::move(*checked15));
if (!checked16) return std::unexpected(std::move(checked16.error()));
identity.gender = *checked16;
}
  {
auto checked17 = config.getString("hairGroup", "hair");
if (!checked17) return std::unexpected(std::move(checked17.error()));
identity.hairGroup = std::move(*checked17);
}
  {
auto checked18 = config.getString("hairType", "male1");
if (!checked18) return std::unexpected(std::move(checked18.error()));
identity.hairType = std::move(*checked18);
}
  {
auto checked19 = config.getString("hairDirectives", "");
if (!checked19) return std::unexpected(std::move(checked19.error()));
identity.hairDirectives = std::move(*checked19);
}
  {
auto checked20 = config.getString("bodyDirectives", "");
if (!checked20) return std::unexpected(std::move(checked20.error()));
identity.bodyDirectives = std::move(*checked20);
}
  {
auto checked21 = config.optString("emoteDirectives");
if (!checked21) return std::unexpected(std::move(checked21.error()));
if (auto jEmoteDirectives = std::move(*checked21)) // Passing Directives as a default arg would be inefficient
    {
auto checked22 = takeOptional(jEmoteDirectives);
if (!checked22) return std::unexpected(std::move(checked22.error()));
identity.emoteDirectives = std::move(*checked22);
}
  else
    identity.emoteDirectives = identity.bodyDirectives;
}

  {
auto checked23 = config.getString("facialHairGroup", "");
if (!checked23) return std::unexpected(std::move(checked23.error()));
identity.facialHairGroup = std::move(*checked23);
}
  {
auto checked24 = config.getString("facialHairType", "");
if (!checked24) return std::unexpected(std::move(checked24.error()));
identity.facialHairType = std::move(*checked24);
}
  {
auto checked25 = config.getString("facialHairDirectives", "");
if (!checked25) return std::unexpected(std::move(checked25.error()));
identity.facialHairDirectives = std::move(*checked25);
}
  {
auto checked26 = config.getString("facialMaskGroup", "");
if (!checked26) return std::unexpected(std::move(checked26.error()));
identity.facialMaskGroup = std::move(*checked26);
}
  {
auto checked27 = config.getString("facialMaskType", "");
if (!checked27) return std::unexpected(std::move(checked27.error()));
identity.facialMaskType = std::move(*checked27);
}
  {
auto checked28 = config.getString("facialMaskDirectives", "");
if (!checked28) return std::unexpected(std::move(checked28.error()));
identity.facialMaskDirectives = std::move(*checked28);
}

  {
auto checked29 = config.getString("personalityIdle", "idle.1");
if (!checked29) return std::unexpected(std::move(checked29.error()));
identity.personality.idle = std::move(*checked29);
}
  {
auto checked30 = config.getString("personalityArmIdle", "idle.1");
if (!checked30) return std::unexpected(std::move(checked30.error()));
identity.personality.armIdle = std::move(*checked30);
}
  {
auto checked31 = config.get("personalityHeadOffset", JsonArray{0, 0});
if (!checked31) return std::unexpected(std::move(checked31.error()));
auto checked32 = jsonToVec2F(std::move(*checked31));
if (!checked32) return std::unexpected(std::move(checked32.error()));
identity.personality.headOffset = std::move(*checked32);
}
  {
auto checked33 = config.get("personalityArmOffset", JsonArray{0, 0});
if (!checked33) return std::unexpected(std::move(checked33.error()));
auto checked34 = jsonToVec2F(std::move(*checked33));
if (!checked34) return std::unexpected(std::move(checked34.error()));
identity.personality.armOffset = std::move(*checked34);
}

  {
auto checked35 = config.get("color", JsonArray({51, 117, 237, 255}));
if (!checked35) return std::unexpected(std::move(checked35.error()));
auto checked36 = jsonToColor(std::move(*checked35));
if (!checked36) return std::unexpected(std::move(checked36.error()));
identity.color = checked36->toRgba();
}

  {
auto checked38 = config.optString("imagePath");
if (!checked38) return std::unexpected(std::move(checked38.error()));
identity.imagePath = std::move(*checked38);
}

  return identity;
}

std::expected<Json, StarException> HumanoidIdentity::toJson() const {
  auto checked39 = enumName<GenderNames>(gender);
  if (!checked39)
    return std::unexpected(std::move(checked39.error()));
  auto result = JsonObject{
    {"name", name},
    {"species", species},
    {"gender", String(*checked39)},
    {"hairGroup", hairGroup},
    {"hairType", hairType},
    {"hairDirectives", hairDirectives.string()},
    {"bodyDirectives", bodyDirectives.string()},
    {"emoteDirectives", emoteDirectives.string()},
    {"facialHairGroup", facialHairGroup},
    {"facialHairType", facialHairType},
    {"facialHairDirectives", facialHairDirectives.string()},
    {"facialMaskGroup", facialMaskGroup},
    {"facialMaskType", facialMaskType},
    {"facialMaskDirectives", facialMaskDirectives.string()},
    {"personalityIdle", personality.idle},
    {"personalityArmIdle", personality.armIdle},
    {"personalityHeadOffset", jsonFromVec2F(personality.headOffset)},
    {"personalityArmOffset", jsonFromVec2F(personality.armOffset)},
    {"color", jsonFromColor(Color::rgba(color))},
  };
  if (imagePath)
    result["imagePath"] = *imagePath;
  return result;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, HumanoidIdentity& identity) {
  {
auto checked40 = ds.read(identity.name);
if (!checked40) return std::unexpected(std::move(checked40.error()));
}
  {
auto checked41 = ds.read(identity.species);
if (!checked41) return std::unexpected(std::move(checked41.error()));
}
  {
auto checked42 = ds.read(identity.gender);
if (!checked42) return std::unexpected(std::move(checked42.error()));
}
  {
auto checked43 = ds.read(identity.hairGroup);
if (!checked43) return std::unexpected(std::move(checked43.error()));
}
  {
auto checked44 = ds.read(identity.hairType);
if (!checked44) return std::unexpected(std::move(checked44.error()));
}
  {
auto checked45 = ds.read(identity.hairDirectives);
if (!checked45) return std::unexpected(std::move(checked45.error()));
}
  {
auto checked46 = ds.read(identity.bodyDirectives);
if (!checked46) return std::unexpected(std::move(checked46.error()));
}
  {
auto checked47 = ds.read(identity.emoteDirectives);
if (!checked47) return std::unexpected(std::move(checked47.error()));
}
  {
auto checked48 = ds.read(identity.facialHairGroup);
if (!checked48) return std::unexpected(std::move(checked48.error()));
}
  {
auto checked49 = ds.read(identity.facialHairType);
if (!checked49) return std::unexpected(std::move(checked49.error()));
}
  {
auto checked50 = ds.read(identity.facialHairDirectives);
if (!checked50) return std::unexpected(std::move(checked50.error()));
}
  {
auto checked51 = ds.read(identity.facialMaskGroup);
if (!checked51) return std::unexpected(std::move(checked51.error()));
}
  {
auto checked52 = ds.read(identity.facialMaskType);
if (!checked52) return std::unexpected(std::move(checked52.error()));
}
  {
auto checked53 = ds.read(identity.facialMaskDirectives);
if (!checked53) return std::unexpected(std::move(checked53.error()));
}
  {
auto checked54 = ds.read(identity.personality.idle);
if (!checked54) return std::unexpected(std::move(checked54.error()));
}
  {
auto checked55 = ds.read(identity.personality.armIdle);
if (!checked55) return std::unexpected(std::move(checked55.error()));
}
  {
auto checked56 = ds.read(identity.personality.headOffset);
if (!checked56) return std::unexpected(std::move(checked56.error()));
}
  {
auto checked57 = ds.read(identity.personality.armOffset);
if (!checked57) return std::unexpected(std::move(checked57.error()));
}
  {
auto checked58 = ds.read(identity.color);
if (!checked58) return std::unexpected(std::move(checked58.error()));
}
  {
auto checked59 = ds.read(identity.imagePath);
if (!checked59) return std::unexpected(std::move(checked59.error()));
}

  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, HumanoidIdentity const& identity) {
  {
auto checked60 = ds.write(identity.name);
if (!checked60) return std::unexpected(std::move(checked60.error()));
}
  {
auto checked61 = ds.write(identity.species);
if (!checked61) return std::unexpected(std::move(checked61.error()));
}
  {
auto checked62 = ds.write(identity.gender);
if (!checked62) return std::unexpected(std::move(checked62.error()));
}
  {
auto checked63 = ds.write(identity.hairGroup);
if (!checked63) return std::unexpected(std::move(checked63.error()));
}
  {
auto checked64 = ds.write(identity.hairType);
if (!checked64) return std::unexpected(std::move(checked64.error()));
}
  {
auto checked65 = ds.write(identity.hairDirectives);
if (!checked65) return std::unexpected(std::move(checked65.error()));
}
  {
auto checked66 = ds.write(identity.bodyDirectives);
if (!checked66) return std::unexpected(std::move(checked66.error()));
}
  {
auto checked67 = ds.write(identity.emoteDirectives);
if (!checked67) return std::unexpected(std::move(checked67.error()));
}
  {
auto checked68 = ds.write(identity.facialHairGroup);
if (!checked68) return std::unexpected(std::move(checked68.error()));
}
  {
auto checked69 = ds.write(identity.facialHairType);
if (!checked69) return std::unexpected(std::move(checked69.error()));
}
  {
auto checked70 = ds.write(identity.facialHairDirectives);
if (!checked70) return std::unexpected(std::move(checked70.error()));
}
  {
auto checked71 = ds.write(identity.facialMaskGroup);
if (!checked71) return std::unexpected(std::move(checked71.error()));
}
  {
auto checked72 = ds.write(identity.facialMaskType);
if (!checked72) return std::unexpected(std::move(checked72.error()));
}
  {
auto checked73 = ds.write(identity.facialMaskDirectives);
if (!checked73) return std::unexpected(std::move(checked73.error()));
}
  {
auto checked74 = ds.write(identity.personality.idle);
if (!checked74) return std::unexpected(std::move(checked74.error()));
}
  {
auto checked75 = ds.write(identity.personality.armIdle);
if (!checked75) return std::unexpected(std::move(checked75.error()));
}
  {
auto checked76 = ds.write(identity.personality.headOffset);
if (!checked76) return std::unexpected(std::move(checked76.error()));
}
  {
auto checked77 = ds.write(identity.personality.armOffset);
if (!checked77) return std::unexpected(std::move(checked77.error()));
}
  {
auto checked78 = ds.write(identity.color);
if (!checked78) return std::unexpected(std::move(checked78.error()));
}
  {
auto checked79 = ds.write(identity.imagePath);
if (!checked79) return std::unexpected(std::move(checked79.error()));
}

  return std::ref(ds);
}

std::expected<Humanoid::HumanoidTiming, StarException> Humanoid::HumanoidTiming::create(Root& root, Json config) {
  HumanoidTiming timing;
  if (config.type() != Json::Type::Object) {
    auto checked80 = root.assets();
if (!checked80) return std::unexpected(std::move(checked80.error()));
auto assets = std::move(*checked80);
    auto configResult = assets->json("/humanoid.config:humanoidTiming");
    if (!configResult) return std::unexpected(std::move(configResult.error()));
    config = std::move(*configResult);
  }

  {
auto checked81 = config.contains("stateCycle");
if (!checked81) return std::unexpected(std::move(checked81.error()));
if (std::move(*checked81))
    {
auto checked82 = config.get("stateCycle");
if (!checked82) return std::unexpected(std::move(checked82.error()));
auto checked83 = jsonToArrayF<STATESIZE>(std::move(*checked82));
if (!checked83) return std::unexpected(std::move(checked83.error()));
timing.stateCycle = std::move(*checked83);
}
}
  {
auto checked84 = config.contains("stateFrames");
if (!checked84) return std::unexpected(std::move(checked84.error()));
if (std::move(*checked84))
    {
auto checked85 = config.get("stateFrames");
if (!checked85) return std::unexpected(std::move(checked85.error()));
auto checked86 = jsonToArrayU<STATESIZE>(std::move(*checked85));
if (!checked86) return std::unexpected(std::move(checked86.error()));
timing.stateFrames = std::move(*checked86);
}
}

  {
auto checked87 = config.contains("emoteCycle");
if (!checked87) return std::unexpected(std::move(checked87.error()));
if (std::move(*checked87))
    {
auto checked88 = config.get("emoteCycle");
if (!checked88) return std::unexpected(std::move(checked88.error()));
auto checked89 = jsonToArrayF<EmoteSize>(std::move(*checked88));
if (!checked89) return std::unexpected(std::move(checked89.error()));
timing.emoteCycle = std::move(*checked89);
}
}
  {
auto checked90 = config.contains("emoteFrames");
if (!checked90) return std::unexpected(std::move(checked90.error()));
if (std::move(*checked90))
    {
auto checked91 = config.get("emoteFrames");
if (!checked91) return std::unexpected(std::move(checked91.error()));
auto checked92 = jsonToArrayU<EmoteSize>(std::move(*checked91));
if (!checked92) return std::unexpected(std::move(checked92.error()));
timing.emoteFrames = std::move(*checked92);
}
}
  return timing;
}

bool Humanoid::HumanoidTiming::cyclicState(State state) {
  switch (state) {
    case State::Walk:
    case State::Run:
    case State::Swim:
      return true;
    default:
      return false;
  }
}

bool Humanoid::HumanoidTiming::cyclicEmoteState(HumanoidEmote state) {
  switch (state) {
    case HumanoidEmote::Blabbering:
    case HumanoidEmote::Shouting:
    case HumanoidEmote::Sad:
    case HumanoidEmote::Laugh:
    case HumanoidEmote::Eat:
    case HumanoidEmote::Sleep:
      return true;
    default:
      return false;
  }
}

int Humanoid::HumanoidTiming::stateSeq(float timer, State state) const {
  return genericSeq(timer, stateCycle[state], stateFrames[state], cyclicState(state));
}

int Humanoid::HumanoidTiming::emoteStateSeq(float timer, HumanoidEmote state) const {
  return genericSeq(timer, emoteCycle[(std::size_t)state], emoteFrames[(std::size_t)state], cyclicEmoteState(state));
}

int Humanoid::HumanoidTiming::danceSeq(float timer, std::shared_ptr<Dance> dance) const {
  return genericSeq(timer, dance->cycle, dance->steps.size(), dance->cyclic) - 1;
}

int Humanoid::HumanoidTiming::genericSeq(float timer, float cycle, unsigned frames, bool cyclic) const {
  if (cyclic) {
    timer = std::fmod(timer, cycle);
  }
  return clamp<int>(timer * frames / cycle, 0, frames - 1) + 1;
}

std::expected<EnumMap<Humanoid::State>, StarException> const Humanoid::StateNames = EnumMap<Humanoid::State>::create({
    {Humanoid::State::Idle, "idle"},
    {Humanoid::State::Walk, "walk"},
    {Humanoid::State::Run, "run"},
    {Humanoid::State::Jump, "jump"},
    {Humanoid::State::Fall, "fall"},
    {Humanoid::State::Swim, "swim"},
    {Humanoid::State::SwimIdle, "swimIdle"},
    {Humanoid::State::Duck, "duck"},
    {Humanoid::State::Sit, "sit"},
    {Humanoid::State::Lay, "lay"},
});


Humanoid::Humanoid(Root& root)
  : m_root(root), m_useAnimation(false) {
  m_fashion = std::make_shared<Fashion>();

  m_twoHanded = false;
  m_primaryHand.holdingItem = false;
  m_altHand.holdingItem = false;

  m_movingBackwards = false;
  m_altHand.angle = 0;
  m_facingDirection = Direction::Left;
  m_headRotationTarget = m_headRotation = m_rotation = 0;
  m_scale = Vec2F::filled(1.f);
  m_drawVaporTrail = false;
  m_state = State::Idle;
  m_emoteState = HumanoidEmote::Idle;
  m_dance = {};

  m_primaryHand.angle = 0;
  m_animationTimer = m_emoteAnimationTimer = m_danceTimer = 0.0f;
}

std::expected<std::shared_ptr<Humanoid>, StarException> Humanoid::create(Root& root) {
  auto humanoid = std::shared_ptr<Humanoid>(new Humanoid(root));
  auto timing = HumanoidTiming::create(root, {});
  if (!timing) return std::unexpected(std::move(timing.error()));
  humanoid->m_timing = std::move(*timing);
  auto animator = NetworkedAnimator::create(root);
  if (!animator) return std::unexpected(std::move(animator.error()));
  humanoid->m_networkedAnimator = std::move(*animator);
  return humanoid;
}

std::expected<std::shared_ptr<Humanoid>, StarException> Humanoid::create(Humanoid const& source) {
  auto animator = NetworkedAnimator::create(*source.m_networkedAnimator);
  if (!animator) return std::unexpected(std::move(animator.error()));
  auto humanoid = std::shared_ptr<Humanoid>(new Humanoid(source));
  humanoid->m_networkedAnimator = std::move(*animator);
  return humanoid;
}

std::expected<std::shared_ptr<Humanoid>, StarException> Humanoid::create(Root& root, Json const& config) {
  auto humanoid = create(root);
  if (!humanoid) return std::unexpected(std::move(humanoid.error()));
  (*humanoid)->m_baseConfig = config;
  auto loaded = (*humanoid)->loadConfig(JsonObject());
  if (!loaded) return std::unexpected(std::move(loaded.error()));
  auto animated = (*humanoid)->loadAnimation();
  if (!animated) return std::unexpected(std::move(animated.error()));
  return humanoid;
}

std::expected<std::shared_ptr<Humanoid>, StarException> Humanoid::create(Root& root, HumanoidIdentity const& identity, JsonObject parameters, Json config, std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog) {
  auto humanoid = create(root);
  if (!humanoid) return std::unexpected(std::move(humanoid.error()));
  auto database = root.speciesDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  auto base = (*database)->humanoidConfig(identity, parameters, config, itemCatalog);
  if (!base) return std::unexpected(std::move(base.error()));
  (*humanoid)->m_identity = identity;
  (*humanoid)->m_baseConfig = std::move(*base);
  auto loaded = (*humanoid)->loadConfig(JsonObject());
  if (!loaded) return std::unexpected(std::move(loaded.error()));
  auto animated = (*humanoid)->loadAnimation();
  if (!animated) return std::unexpected(std::move(animated.error()));
  auto identified = (*humanoid)->setIdentity(identity);
  if (!identified) return std::unexpected(std::move(identified.error()));
  return humanoid;
}

std::expected<void, StarException> Humanoid::setIdentity(HumanoidIdentity const& identity) {
  auto checked93 = enumName<GenderNames>(identity.gender);
  if (!checked93)
    return std::unexpected(std::move(checked93.error()));
  m_genderName = String(*checked93);
  m_identity = identity;
  m_headFrameset = getHeadFromIdentity();
  m_bodyFrameset = getBodyFromIdentity();
  m_emoteFrameset = getFacialEmotesFromIdentity();
  m_hairFrameset = getHairFromIdentity();
  m_facialHairFrameset = getFacialHairFromIdentity();
  m_facialMaskFrameset = getFacialMaskFromIdentity();
  m_backArmFrameset = getBackArmFromIdentity();
  m_frontArmFrameset = getFrontArmFromIdentity();
  m_vaporTrailFrameset = getVaporTrailFrameset();
  if (m_useBodyMask) {
    m_bodyMaskFrameset = getBodyMaskFromIdentity();
    
}
  if (m_useBodyHeadMask) {
    m_bodyHeadMaskFrameset = getBodyHeadMaskFromIdentity();
    
}
  if (m_useAnimation) {
    m_networkedAnimator->setLocalTag("name", m_identity.name);
    m_networkedAnimator->setLocalTag("species", m_identity.species);
    m_networkedAnimator->setLocalTag("gender", m_genderName);
    m_networkedAnimator->setLocalTag("hairGroup", m_identity.hairGroup);
    m_networkedAnimator->setLocalTag("hairType", m_identity.hairType);
    m_networkedAnimator->setLocalTag("hairDirectives", m_identity.hairDirectives.string());
    m_networkedAnimator->setLocalTag("bodyDirectives", m_identity.bodyDirectives.string());
    m_networkedAnimator->setLocalTag("emoteDirectives", m_identity.emoteDirectives.string());
    m_networkedAnimator->setLocalTag("facialHairGroup", m_identity.facialHairGroup);
    m_networkedAnimator->setLocalTag("facialHairType", m_identity.facialHairType);
    m_networkedAnimator->setLocalTag("facialHairDirectives", m_identity.facialHairDirectives.string());
    m_networkedAnimator->setLocalTag("facialMaskGroup", m_identity.facialHairGroup);
    m_networkedAnimator->setLocalTag("facialMaskType", m_identity.facialHairType);
    m_networkedAnimator->setLocalTag("facialMaskDirectives", m_identity.facialMaskDirectives.string());
    m_networkedAnimator->setLocalTag("personalityIdle", m_identity.personality.idle);
    m_networkedAnimator->setLocalTag("personalityArmIdle", m_identity.personality.armIdle);
    auto checked94 = m_networkedAnimator->resetLocalTransformationGroup("personalityHeadOffset");
if (!checked94) return std::unexpected(std::move(checked94.error()));
    auto checked95 = m_networkedAnimator->translateLocalTransformationGroup("personalityHeadOffset", m_identity.personality.headOffset / TilePixels);
if (!checked95) return std::unexpected(std::move(checked95.error()));
    auto checked96 = m_networkedAnimator->resetLocalTransformationGroup("personalityArmOffset");
if (!checked96) return std::unexpected(std::move(checked96.error()));
    auto checked97 = m_networkedAnimator->translateLocalTransformationGroup("personalityArmOffset", m_identity.personality.armOffset / TilePixels);
if (!checked97) return std::unexpected(std::move(checked97.error()));

    m_networkedAnimator->setLocalTag("hairFrameset", m_identity.hairType.empty() ? "" : (String)strf("{}/{}.png", m_identity.hairGroup, m_identity.hairType));
    m_networkedAnimator->setLocalTag("facialHairFrameset", m_identity.facialHairType.empty() ? "" : (String)strf("{}/{}.png", m_identity.facialHairGroup, m_identity.facialHairType));
    m_networkedAnimator->setLocalTag("facialMaskFrameset", m_identity.facialMaskType.empty() ? "" : (String)strf("{}/{}.png", m_identity.facialMaskGroup, m_identity.facialMaskType));

    for (auto p : m_identityFramesetTags) {
      auto checked98 = m_networkedAnimator->applyPartTags("anchor", p.first);
if (!checked98) return std::unexpected(std::move(checked98.error()));
auto checked99 = m_networkedAnimator->applyPartTags("anchor", p.second);
if (!checked99) return std::unexpected(std::move(checked99.error()));
m_networkedAnimator->setLocalTag(std::move(*checked98), std::move(*checked99));
      
}
    
}
  return {};
}

HumanoidIdentity const& Humanoid::identity() const {
  return m_identity;
}

std::expected<void, StarException> Humanoid::setHumanoidParameters(JsonObject parameters){
  m_baseConfig = jsonMerge(m_baseConfig, parameters);
  auto checked100 = loadConfig(std::exchange(m_mergeConfig, {}), true);
  if (!checked100)
    return std::unexpected(std::move(checked100.error()));

  return {};
}

std::expected<bool, StarException> Humanoid::loadConfig(Json merger, bool forceRefresh) {
  if (m_mergeConfig == merger && !forceRefresh)
    return false;

  auto config = jsonMerge(m_baseConfig, merger);
  auto checked101 = config.getObject("humanoidTiming");
if (!checked101) return std::unexpected(std::move(checked101.error()));
auto checked102 = HumanoidTiming::create(m_root, std::move(*checked101));
if (!checked102) return std::unexpected(std::move(checked102.error()));
m_timing = std::move(*checked102);

  auto checked103 = config.get("globalOffset");
if (!checked103) return std::unexpected(std::move(checked103.error()));
auto checked104 = jsonToVec2F(std::move(*checked103));
if (!checked104) return std::unexpected(std::move(checked104.error()));
m_globalOffset = std::move(*checked104) / TilePixels;
  auto checked105 = config.get("headRunOffset");
if (!checked105) return std::unexpected(std::move(checked105.error()));
auto checked106 = jsonToVec2F(std::move(*checked105));
if (!checked106) return std::unexpected(std::move(checked106.error()));
m_headRunOffset = std::move(*checked106) / TilePixels;
  auto checked107 = config.get("headSwimOffset");
if (!checked107) return std::unexpected(std::move(checked107.error()));
auto checked108 = jsonToVec2F(std::move(*checked107));
if (!checked108) return std::unexpected(std::move(checked108.error()));
m_headSwimOffset = std::move(*checked108) / TilePixels;
  auto checked109 = config.get("runFallOffset");
if (!checked109) return std::unexpected(std::move(checked109.error()));
auto checked110 = std::move(*checked109).toDouble();
if (!checked110) return std::unexpected(std::move(checked110.error()));
m_runFallOffset = std::move(*checked110) / TilePixels;
  auto checked111 = config.get("duckOffset");
if (!checked111) return std::unexpected(std::move(checked111.error()));
auto checked112 = std::move(*checked111).toDouble();
if (!checked112) return std::unexpected(std::move(checked112.error()));
m_duckOffset = std::move(*checked112) / TilePixels;
  auto checked113 = config.get("headDuckOffset");
if (!checked113) return std::unexpected(std::move(checked113.error()));
auto checked114 = jsonToVec2F(std::move(*checked113));
if (!checked114) return std::unexpected(std::move(checked114.error()));
m_headDuckOffset = std::move(*checked114) / TilePixels;
  auto checked115 = config.get("sitOffset");
if (!checked115) return std::unexpected(std::move(checked115.error()));
auto checked116 = std::move(*checked115).toDouble();
if (!checked116) return std::unexpected(std::move(checked116.error()));
m_sitOffset = std::move(*checked116) / TilePixels;
  auto checked117 = config.get("layOffset");
if (!checked117) return std::unexpected(std::move(checked117.error()));
auto checked118 = std::move(*checked117).toDouble();
if (!checked118) return std::unexpected(std::move(checked118.error()));
m_layOffset = std::move(*checked118) / TilePixels;
  auto checked119 = config.get("headSitOffset");
if (!checked119) return std::unexpected(std::move(checked119.error()));
auto checked120 = jsonToVec2F(std::move(*checked119));
if (!checked120) return std::unexpected(std::move(checked120.error()));
m_headSitOffset = std::move(*checked120) / TilePixels;
  auto checked121 = config.get("headLayOffset");
if (!checked121) return std::unexpected(std::move(checked121.error()));
auto checked122 = jsonToVec2F(std::move(*checked121));
if (!checked122) return std::unexpected(std::move(checked122.error()));
m_headLayOffset = std::move(*checked122) / TilePixels;
  auto checked123 = config.get("recoilOffset");
if (!checked123) return std::unexpected(std::move(checked123.error()));
auto checked124 = jsonToVec2F(std::move(*checked123));
if (!checked124) return std::unexpected(std::move(checked124.error()));
m_recoilOffset = std::move(*checked124) / TilePixels;
  auto checked125 = config.get("mouthOffset");
if (!checked125) return std::unexpected(std::move(checked125.error()));
auto checked126 = jsonToVec2F(std::move(*checked125));
if (!checked126) return std::unexpected(std::move(checked126.error()));
m_mouthOffset = std::move(*checked126) / TilePixels;
  auto checked127 = config.get("feetOffset");
if (!checked127) return std::unexpected(std::move(checked127.error()));
auto checked128 = jsonToVec2F(std::move(*checked127));
if (!checked128) return std::unexpected(std::move(checked128.error()));
m_feetOffset = std::move(*checked128) / TilePixels;

  auto checked129 = config.getBool("bodyFullbright", false);
if (!checked129) return std::unexpected(std::move(checked129.error()));
m_bodyFullbright = std::move(*checked129);
  auto checked130 = config.getBool("useBodyMask", false);
if (!checked130) return std::unexpected(std::move(checked130.error()));
m_useBodyMask = std::move(*checked130);
  auto checked131 = config.getBool("useBodyHeadMask", false);
if (!checked131) return std::unexpected(std::move(checked131.error()));
m_useBodyHeadMask = std::move(*checked131);

  auto checked132 = config.get("headArmorOffset");
if (!checked132) return std::unexpected(std::move(checked132.error()));
auto checked133 = jsonToVec2F(std::move(*checked132));
if (!checked133) return std::unexpected(std::move(checked133.error()));
m_headArmorOffset = std::move(*checked133) / TilePixels;
  auto checked134 = config.get("chestArmorOffset");
if (!checked134) return std::unexpected(std::move(checked134.error()));
auto checked135 = jsonToVec2F(std::move(*checked134));
if (!checked135) return std::unexpected(std::move(checked135.error()));
m_chestArmorOffset = std::move(*checked135) / TilePixels;
  auto checked136 = config.get("legsArmorOffset");
if (!checked136) return std::unexpected(std::move(checked136.error()));
auto checked137 = jsonToVec2F(std::move(*checked136));
if (!checked137) return std::unexpected(std::move(checked137.error()));
m_legsArmorOffset = std::move(*checked137) / TilePixels;
  auto checked138 = config.get("backArmorOffset");
if (!checked138) return std::unexpected(std::move(checked138.error()));
auto checked139 = jsonToVec2F(std::move(*checked138));
if (!checked139) return std::unexpected(std::move(checked139.error()));
m_backArmorOffset = std::move(*checked139) / TilePixels;

  auto checked140 = config.getBool("bodyHidden", false);
if (!checked140) return std::unexpected(std::move(checked140.error()));
m_bodyHidden = std::move(*checked140);

  auto checked141 = config.get("armWalkSeq");
if (!checked141) return std::unexpected(std::move(checked141.error()));
auto checked142 = jsonToIntList(std::move(*checked141));
if (!checked142) return std::unexpected(std::move(checked142.error()));
m_armWalkSeq = std::move(*checked142);
  auto checked143 = config.get("armRunSeq");
if (!checked143) return std::unexpected(std::move(checked143.error()));
auto checked144 = jsonToIntList(std::move(*checked143));
if (!checked144) return std::unexpected(std::move(checked144.error()));
m_armRunSeq = std::move(*checked144);

  m_walkBob.clear();
  {
auto checked145 = config.get("walkBob");
if (!checked145) return std::unexpected(std::move(checked145.error()));
auto checked146 = std::move(*checked145).toArray();
if (!checked146) return std::unexpected(std::move(checked146.error()));
for (auto const& v : std::move(*checked146))
    {
auto checked147 = v.toDouble();
if (!checked147) return std::unexpected(std::move(checked147.error()));
m_walkBob.append(std::move(*checked147) / TilePixels);
}
}
  m_runBob.clear();
  {
auto checked148 = config.get("runBob");
if (!checked148) return std::unexpected(std::move(checked148.error()));
auto checked149 = std::move(*checked148).toArray();
if (!checked149) return std::unexpected(std::move(checked149.error()));
for (auto const& v : std::move(*checked149))
    {
auto checked150 = v.toDouble();
if (!checked150) return std::unexpected(std::move(checked150.error()));
m_runBob.append(std::move(*checked150) / TilePixels);
}
}
  m_swimBob.clear();
  {
auto checked151 = config.get("swimBob");
if (!checked151) return std::unexpected(std::move(checked151.error()));
auto checked152 = std::move(*checked151).toArray();
if (!checked152) return std::unexpected(std::move(checked152.error()));
for (auto const& v : std::move(*checked152))
    {
auto checked153 = v.toDouble();
if (!checked153) return std::unexpected(std::move(checked153.error()));
m_swimBob.append(std::move(*checked153) / TilePixels);
}
}

  auto checked154 = config.get("jumpBob");
if (!checked154) return std::unexpected(std::move(checked154.error()));
auto checked155 = std::move(*checked154).toDouble();
if (!checked155) return std::unexpected(std::move(checked155.error()));
m_jumpBob = std::move(*checked155) / TilePixels;
  auto checked156 = config.get("frontArmRotationCenter");
if (!checked156) return std::unexpected(std::move(checked156.error()));
auto checked157 = jsonToVec2F(std::move(*checked156));
if (!checked157) return std::unexpected(std::move(checked157.error()));
m_frontArmRotationCenter = std::move(*checked157) / TilePixels;
  auto checked158 = config.get("backArmRotationCenter");
if (!checked158) return std::unexpected(std::move(checked158.error()));
auto checked159 = jsonToVec2F(std::move(*checked158));
if (!checked159) return std::unexpected(std::move(checked159.error()));
m_backArmRotationCenter = std::move(*checked159) / TilePixels;
  auto checked160 = config.get("frontHandPosition");
if (!checked160) return std::unexpected(std::move(checked160.error()));
auto checked161 = jsonToVec2F(std::move(*checked160));
if (!checked161) return std::unexpected(std::move(checked161.error()));
m_frontHandPosition = std::move(*checked161) / TilePixels;
  auto checked162 = config.get("backArmOffset");
if (!checked162) return std::unexpected(std::move(checked162.error()));
auto checked163 = jsonToVec2F(std::move(*checked162));
if (!checked163) return std::unexpected(std::move(checked163.error()));
m_backArmOffset = std::move(*checked163) / TilePixels;
  auto checked164 = config.get("vaporTrailFrames");
if (!checked164) return std::unexpected(std::move(checked164.error()));
auto checked165 = std::move(*checked164).toInt();
if (!checked165) return std::unexpected(std::move(checked165.error()));
m_vaporTrailFrames = std::move(*checked165);
  auto checked166 = config.get("vaporTrailCycle");
if (!checked166) return std::unexpected(std::move(checked166.error()));
auto checked167 = std::move(*checked166).toFloat();
if (!checked167) return std::unexpected(std::move(checked167.error()));
m_vaporTrailCycle = std::move(*checked167);

  auto checked168 = config.getString("deathParticles");
if (!checked168) return std::unexpected(std::move(checked168.error()));
m_defaultDeathParticles = std::move(*checked168);
  auto checked169 = config.get("particleEmitters");
if (!checked169) return std::unexpected(std::move(checked169.error()));
m_particleEmitters = std::move(*checked169);

  auto checked170 = config.get("movementParameters");
if (!checked170) return std::unexpected(std::move(checked170.error()));
Json newMovementParameters = std::move(*checked170);
  auto checked171 = config.opt("playerMovementParameters");
if (!checked171) return std::unexpected(std::move(checked171.error()));
std::optional<Json> newPlayerMovementParameters = std::move(*checked171);
  bool movementParametersChanged = m_defaultMovementParameters != newMovementParameters || m_playerMovementParameters != newPlayerMovementParameters;

  m_defaultMovementParameters = newMovementParameters;
  m_playerMovementParameters = newPlayerMovementParameters;

  m_mergeConfig = merger;


  auto checked172 = config.getArray("headRotationCenter", JsonArray{0,-2});
if (!checked172) return std::unexpected(std::move(checked172.error()));
auto checked173 = jsonToVec2F(std::move(*checked172));
if (!checked173) return std::unexpected(std::move(checked173.error()));
m_headRotationCenter = std::move(*checked173) / TilePixels;

  return movementParametersChanged;
}

std::expected<void, StarException> Humanoid::loadAnimation() {
  auto checked174 = m_baseConfig.opt("animation");
if (!checked174) return std::unexpected(std::move(checked174.error()));
auto animationConfig = std::move(*checked174);
  m_useAnimation = animationConfig.has_value();

  auto checked175 = m_baseConfig.getArray("animationScripts", JsonArray());
if (!checked175) return std::unexpected(std::move(checked175.error()));
auto checked176 = jsonToStringList(std::move(*checked175));
if (!checked176) return std::unexpected(std::move(checked176.error()));
m_animationScripts = std::move(*checked176);

  m_animationStates.clear();
  m_animationStatesBackwards.clear();
  m_emoteAnimationStates.clear();
  m_portraitAnimationStates.clear();

  String animationPath = ("/humanoid/" + m_identity.imagePath.value_or(m_identity.species) + "/");
  if (m_useAnimation)
    {
auto checked177 = NetworkedAnimator::create(m_root, *animationConfig, animationPath);
if (!checked177) return std::unexpected(std::move(checked177.error()));
m_networkedAnimator = std::move(*checked177);
}
  else
    {
auto checked178 = NetworkedAnimator::create(m_root);
if (!checked178) return std::unexpected(std::move(checked178.error()));
m_networkedAnimator = std::move(*checked178);
}

  if (m_useAnimation) {
    auto checked179 = m_baseConfig.getString("frontHandItemPart", "frontHandItem");
if (!checked179) return std::unexpected(std::move(checked179.error()));
m_frontItemPart = std::move(*checked179);
    auto checked180 = m_baseConfig.getString("backHandItemPart", "backHandItem");
if (!checked180) return std::unexpected(std::move(checked180.error()));
m_backItemPart = std::move(*checked180);

    auto checked181 = m_baseConfig.getString("headRotationPart", "head");
if (!checked181) return std::unexpected(std::move(checked181.error()));
auto checked182 = m_baseConfig.getString("headRotationPartPoint", "rotationCenter");
if (!checked182) return std::unexpected(std::move(checked182.error()));
m_headRotationPoint = {std::move(*checked181), std::move(*checked182)};
    auto checked183 = m_baseConfig.getString("frontArmRotationPart", "frontArm");
if (!checked183) return std::unexpected(std::move(checked183.error()));
auto checked184 = m_baseConfig.getString("frontArmRotationPartPoint", "rotationCenter");
if (!checked184) return std::unexpected(std::move(checked184.error()));
m_frontArmRotationPoint = {std::move(*checked183), std::move(*checked184)};
    auto checked185 = m_baseConfig.getString("backArmRotationPart", "backArm");
if (!checked185) return std::unexpected(std::move(checked185.error()));
auto checked186 = m_baseConfig.getString("backArmRotationPartPoint", "rotationCenter");
if (!checked186) return std::unexpected(std::move(checked186.error()));
m_backArmRotationPoint = {std::move(*checked185), std::move(*checked186)};

    auto checked187 = m_baseConfig.getString("mouthOffsetPart", "head");
if (!checked187) return std::unexpected(std::move(checked187.error()));
auto checked188 = m_baseConfig.getString("mouthOffsetPartPoint", "mouthOffset");
if (!checked188) return std::unexpected(std::move(checked188.error()));
m_mouthOffsetPoint = {std::move(*checked187), std::move(*checked188)};
    auto checked189 = m_baseConfig.getString("headArmorOffsetPart", "headCosmetic");
if (!checked189) return std::unexpected(std::move(checked189.error()));
auto checked190 = m_baseConfig.getString("headArmorOffsetPartPoint", "armorOffset");
if (!checked190) return std::unexpected(std::move(checked190.error()));
m_headArmorOffsetPoint = {std::move(*checked189), std::move(*checked190)};
    auto checked191 = m_baseConfig.getString("chestArmorOffsetPart", "chestCosmetic");
if (!checked191) return std::unexpected(std::move(checked191.error()));
auto checked192 = m_baseConfig.getString("chestArmorOffsetPartPoint", "armorOffset");
if (!checked192) return std::unexpected(std::move(checked192.error()));
m_chestArmorOffsetPoint = {std::move(*checked191), std::move(*checked192)};
    auto checked193 = m_baseConfig.getString("legsArmorOffsetPart", "legsCosmetic");
if (!checked193) return std::unexpected(std::move(checked193.error()));
auto checked194 = m_baseConfig.getString("legsArmorOffsetPartPoint", "armorOffset");
if (!checked194) return std::unexpected(std::move(checked194.error()));
m_legsArmorOffsetPoint = {std::move(*checked193), std::move(*checked194)};
    auto checked195 = m_baseConfig.getString("backArmorOffsetPart", "backCosmetic");
if (!checked195) return std::unexpected(std::move(checked195.error()));
auto checked196 = m_baseConfig.getString("backArmorOffsetPartPoint", "armorOffset");
if (!checked196) return std::unexpected(std::move(checked196.error()));
m_backArmorOffsetPoint = {std::move(*checked195), std::move(*checked196)};
    auto checked197 = m_baseConfig.getString("feetOffsetPart", "body");
if (!checked197) return std::unexpected(std::move(checked197.error()));
auto checked198 = m_baseConfig.getString("feetOffsetPartPoint", "feetOffset");
if (!checked198) return std::unexpected(std::move(checked198.error()));
m_feetOffsetPoint = {std::move(*checked197), std::move(*checked198)};
    auto checked199 = m_baseConfig.getString("throwPart", "head");
if (!checked199) return std::unexpected(std::move(checked199.error()));
auto checked200 = m_baseConfig.getString("throwPartPoint", "mouthOffset");
if (!checked200) return std::unexpected(std::move(checked200.error()));
m_throwPoint = {std::move(*checked199), std::move(*checked200)};
    auto checked201 = m_baseConfig.getString("interactPart", "body");
if (!checked201) return std::unexpected(std::move(checked201.error()));
auto checked202 = m_baseConfig.getString("interactPartPoint", "interact");
if (!checked202) return std::unexpected(std::move(checked202.error()));
m_interactPoint = {std::move(*checked201), std::move(*checked202)};

    auto checked203 = m_baseConfig.getObject("identityFramesetTags", JsonObject());
if (!checked203) return std::unexpected(std::move(checked203.error()));
auto checked204 = jsonToMapV<StringMap<String>>(std::move(*checked203), std::mem_fn(&Json::toString));
if (!checked204) return std::unexpected(std::move(checked204.error()));
m_identityFramesetTags = std::move(*checked204);

    {
auto checked205 = m_baseConfig.getObject("stateAnimations", JsonObject());
if (!checked205) return std::unexpected(std::move(checked205.error()));
for (auto pair : std::move(*checked205)) {
      HashMap<String,AnimationStateArgs> animations;
      {
auto checked206 = pair.second.iterateObject();
if (!checked206) return std::unexpected(std::move(checked206.error()));
for (auto anim : std::move(*checked206)){
        auto checked207 = anim.second.toArray();
if (!checked207) return std::unexpected(std::move(checked207.error()));
auto args = std::move(*checked207);
        auto checked208 = args.at(0);
if (!checked208) return std::unexpected(std::move(checked208.error()));
auto checked209 = checked208->get().toString();
if (!checked209) return std::unexpected(std::move(checked209.error()));
auto checked210 = args.at(1);
if (!checked210) return std::unexpected(std::move(checked210.error()));
auto checked211 = checked210->get().toBool();
if (!checked211) return std::unexpected(std::move(checked211.error()));
auto checked212 = args.at(2);
if (!checked212) return std::unexpected(std::move(checked212.error()));
auto checked213 = checked212->get().toBool();
if (!checked213) return std::unexpected(std::move(checked213.error()));
animations.set(anim.first, {std::move(*checked209), std::move(*checked211), std::move(*checked213)});
        
}
}
      if (!StateNames) return std::unexpected(StateNames.error());
auto checked214 = StateNames->getLeft(pair.first);
if (!checked214) return std::unexpected(std::move(checked214.error()));
m_animationStates.set(checked214->get(), animations);
      
}
}
    {
auto checked215 = m_baseConfig.getObject("stateAnimationsBackwards", JsonObject());
if (!checked215) return std::unexpected(std::move(checked215.error()));
for (auto pair : std::move(*checked215)) {
      HashMap<String,AnimationStateArgs> animations;
      {
auto checked216 = pair.second.iterateObject();
if (!checked216) return std::unexpected(std::move(checked216.error()));
for (auto anim : std::move(*checked216)){
        auto checked217 = anim.second.toArray();
if (!checked217) return std::unexpected(std::move(checked217.error()));
auto args = std::move(*checked217);
        auto checked218 = args.at(0);
if (!checked218) return std::unexpected(std::move(checked218.error()));
auto checked219 = checked218->get().toString();
if (!checked219) return std::unexpected(std::move(checked219.error()));
auto checked220 = args.at(1);
if (!checked220) return std::unexpected(std::move(checked220.error()));
auto checked221 = checked220->get().toBool();
if (!checked221) return std::unexpected(std::move(checked221.error()));
auto checked222 = args.at(2);
if (!checked222) return std::unexpected(std::move(checked222.error()));
auto checked223 = checked222->get().toBool();
if (!checked223) return std::unexpected(std::move(checked223.error()));
animations.set(anim.first, {std::move(*checked219), std::move(*checked221), std::move(*checked223)});
        
}
}
      if (!StateNames) return std::unexpected(StateNames.error());
auto checked224 = StateNames->getLeft(pair.first);
if (!checked224) return std::unexpected(std::move(checked224.error()));
m_animationStatesBackwards.set(checked224->get(), animations);
      
}
}
    {
auto checked225 = m_baseConfig.getObject("emoteAnimations", JsonObject());
if (!checked225) return std::unexpected(std::move(checked225.error()));
for (auto pair : std::move(*checked225)) {
      HashMap<String,AnimationStateArgs> animations;
      {
auto checked226 = pair.second.iterateObject();
if (!checked226) return std::unexpected(std::move(checked226.error()));
for (auto anim : std::move(*checked226)){
        auto checked227 = anim.second.toArray();
if (!checked227) return std::unexpected(std::move(checked227.error()));
auto args = std::move(*checked227);
        auto checked228 = args.at(0);
if (!checked228) return std::unexpected(std::move(checked228.error()));
auto checked229 = checked228->get().toString();
if (!checked229) return std::unexpected(std::move(checked229.error()));
auto checked230 = args.at(1);
if (!checked230) return std::unexpected(std::move(checked230.error()));
auto checked231 = checked230->get().toBool();
if (!checked231) return std::unexpected(std::move(checked231.error()));
auto checked232 = args.at(2);
if (!checked232) return std::unexpected(std::move(checked232.error()));
auto checked233 = checked232->get().toBool();
if (!checked233) return std::unexpected(std::move(checked233.error()));
animations.set(anim.first, {std::move(*checked229), std::move(*checked231), std::move(*checked233)});
        
}
}
      if (!HumanoidEmoteFrameBaseNames) return std::unexpected(HumanoidEmoteFrameBaseNames.error());
auto checked234 = HumanoidEmoteFrameBaseNames->getLeft(pair.first);
if (!checked234) return std::unexpected(std::move(checked234.error()));
m_emoteAnimationStates.set(checked234->get(), animations);
      
}
}
    {
auto checked235 = m_baseConfig.getObject("portraitAnimations", JsonObject());
if (!checked235) return std::unexpected(std::move(checked235.error()));
for (auto pair : std::move(*checked235)) {
      HashMap<String,AnimationStateArgs> animations;
      {
auto checked236 = pair.second.iterateObject();
if (!checked236) return std::unexpected(std::move(checked236.error()));
for (auto anim : std::move(*checked236)){
        auto checked237 = anim.second.toArray();
if (!checked237) return std::unexpected(std::move(checked237.error()));
auto args = std::move(*checked237);
        auto checked238 = args.at(0);
if (!checked238) return std::unexpected(std::move(checked238.error()));
auto checked239 = checked238->get().toString();
if (!checked239) return std::unexpected(std::move(checked239.error()));
auto checked240 = args.at(1);
if (!checked240) return std::unexpected(std::move(checked240.error()));
auto checked241 = checked240->get().toBool();
if (!checked241) return std::unexpected(std::move(checked241.error()));
auto checked242 = args.at(2);
if (!checked242) return std::unexpected(std::move(checked242.error()));
auto checked243 = checked242->get().toBool();
if (!checked243) return std::unexpected(std::move(checked243.error()));
animations.set(anim.first, {std::move(*checked239), std::move(*checked241), std::move(*checked243)});
        
}
}
auto checked244 = enumValue<PortraitModeNames>(pair.first);
if (!checked244) return std::unexpected(std::move(checked244.error()));
m_portraitAnimationStates.set(*checked244, animations);
}
}
    
}
  return {};
}

void Humanoid::wearableRemoved(Wearable const& wearable) {
  auto& fashion = *m_fashion;

  auto setTags = [&](HashMap<String,String> tags) {
    if (m_useAnimation)
      for (auto tag : tags) {
        m_networkedAnimator->setLocalTag(tag.first);
      }
  };

  if (std::holds_alternative<WornHead>(wearable)) {
    auto const& head = std::get<WornHead>(wearable);
    fashion.wornHeadsChanged = true;
    if (!head.maskDirectives.empty())
      fashion.helmetMasksChanged = true;
    setTags(head.animationTags);
  } else if (std::holds_alternative<WornChest>(wearable)) {
    fashion.wornChestsLegsChanged = true;
    setTags(std::get<WornChest>(wearable).animationTags);
  } else if (std::holds_alternative<WornLegs>(wearable)) {
    fashion.wornChestsLegsChanged = true;
    setTags(std::get<WornLegs>(wearable).animationTags);
  } else if (std::holds_alternative<WornBack>(wearable)) {
    fashion.wornBacksChanged = true;
    setTags(std::get<WornBack>(wearable).animationTags);
  }
}

std::expected<void, StarException> Humanoid::removeWearable(std::uint8_t slot) {
  auto& fashion = *m_fashion;
  auto checked245 = fashion.wearables.at(slot);
if (!checked245) return std::unexpected(std::move(checked245.error()));
Wearable& current = checked245->get();
  wearableRemoved(current);
  current.emplace<0>();
  return {};
}

std::expected<void, StarException> Humanoid::setWearableFromHead(std::uint8_t slot, HeadArmor const& head, Gender gender) {
  auto& fashion = *m_fashion;
  auto checked246 = fashion.wearables.at(slot);
if (!checked246) return std::unexpected(std::move(checked246.error()));
Wearable& current = checked246->get();
  if (std::holds_alternative<WornHead>(current))
    fashion.helmetMasksChanged |= std::get<WornHead>(current).maskDirectives != head.maskDirectives();
  else {
    wearableRemoved(current);
    fashion.wornHeadsChanged = true;
    fashion.helmetMasksChanged |= !head.maskDirectives().empty();
  }
  auto& wornHead = current.emplace<WornHead>();
  wornHead.directives = head.directives(m_facingDirection == Direction::Left);
  wornHead.fullbright = head.fullbright();
  wornHead.frameset = head.frameset(gender);
  wornHead.maskDirectives = head.maskDirectives();
  wornHead.animationTags.clear();
  wornHead.animationTags.set(strf("headCosmetic{}Frameset", slot+1), wornHead.frameset);
  wornHead.animationTags.set(strf("headCosmetic{}Directives", slot+1), wornHead.directives.string());
  {
auto checked247 = head.instanceValue("humanoidAnimationTags", JsonObject());
if (!checked247) return std::unexpected(std::move(checked247.error()));
auto checked248 = std::move(*checked247).iterateObject();
if (!checked248) return std::unexpected(std::move(checked248.error()));
for (auto tag : std::move(*checked248)) {
    auto checked249 = tag.first.replace("<slot>", toString(slot + 1));
if (!checked249) return std::unexpected(std::move(checked249.error()));
auto checked250 = m_networkedAnimator->applyPartTags(m_headArmorOffsetPoint.first, std::move(*checked249));
if (!checked250) return std::unexpected(std::move(checked250.error()));
auto checked251 = tag.second.toString();
if (!checked251) return std::unexpected(std::move(checked251.error()));
auto checked252 = m_networkedAnimator->applyPartTags(m_headArmorOffsetPoint.first, std::move(*checked251));
if (!checked252) return std::unexpected(std::move(checked252.error()));
wornHead.animationTags.set(
      std::move(*checked250),
      std::move(*checked252)
    );
  }
}
  return {};
}

std::expected<void, StarException> Humanoid::setWearableFromChest(std::uint8_t slot, ChestArmor const& chest, Gender gender) {
  auto& fashion = *m_fashion;
  auto checked253 = fashion.wearables.at(slot);
if (!checked253) return std::unexpected(std::move(checked253.error()));
Wearable& current = checked253->get();
  if (!std::holds_alternative<WornChest>(current) && !std::holds_alternative<WornLegs>(current)) {
    wearableRemoved(current);
    fashion.wornChestsLegsChanged = true;
  }

  auto& wornChest = current.emplace<WornChest>();
  wornChest.directives = chest.directives(m_facingDirection == Direction::Left);
  wornChest.fullbright = chest.fullbright();
  wornChest.frameset = chest.bodyFrameset(gender);
  wornChest.backSleeveFrameset = chest.backSleeveFrameset(gender);
  wornChest.frontSleeveFrameset = chest.frontSleeveFrameset(gender);
  wornChest.animationTags.clear();
  wornChest.animationTags.set(strf("chestCosmetic{}Frameset", slot+1), wornChest.frameset);
  wornChest.animationTags.set(strf("frontSleeve{}Frameset", slot+1), wornChest.frontSleeveFrameset);
  wornChest.animationTags.set(strf("backSleeve{}Frameset", slot+1), wornChest.backSleeveFrameset);
  wornChest.animationTags.set(strf("chestCosmetic{}Directives", slot+1), wornChest.directives.string());
  {
auto checked254 = chest.instanceValue("humanoidAnimationTags", JsonObject());
if (!checked254) return std::unexpected(std::move(checked254.error()));
auto checked255 = std::move(*checked254).iterateObject();
if (!checked255) return std::unexpected(std::move(checked255.error()));
for (auto tag : std::move(*checked255)) {
    auto checked256 = tag.first.replace("<slot>", toString(slot + 1));
if (!checked256) return std::unexpected(std::move(checked256.error()));
auto checked257 = m_networkedAnimator->applyPartTags(m_chestArmorOffsetPoint.first, std::move(*checked256));
if (!checked257) return std::unexpected(std::move(checked257.error()));
auto checked258 = tag.second.toString();
if (!checked258) return std::unexpected(std::move(checked258.error()));
auto checked259 = m_networkedAnimator->applyPartTags(m_chestArmorOffsetPoint.first, std::move(*checked258));
if (!checked259) return std::unexpected(std::move(checked259.error()));
wornChest.animationTags.set(
      std::move(*checked257),
      std::move(*checked259)
    );
  }
}
  return {};
}

std::expected<void, StarException> Humanoid::setWearableFromLegs(std::uint8_t slot, LegsArmor const& legs, Gender gender) {
  auto& fashion = *m_fashion;
  auto checked260 = fashion.wearables.at(slot);
if (!checked260) return std::unexpected(std::move(checked260.error()));
Wearable& current = checked260->get();
  if (!std::holds_alternative<WornChest>(current) && !std::holds_alternative<WornLegs>(current)) {
    wearableRemoved(current);
    fashion.wornChestsLegsChanged = true;
  }

  auto& wornLegs = current.emplace<WornLegs>();
  wornLegs.directives = legs.directives(m_facingDirection == Direction::Left);
  wornLegs.fullbright = legs.fullbright();
  wornLegs.frameset = legs.frameset(gender);
  wornLegs.animationTags.clear();
  wornLegs.animationTags.set(strf("legsCosmetic{}Frameset", slot+1), wornLegs.frameset);
  wornLegs.animationTags.set(strf("legsCosmetic{}Directives", slot+1), wornLegs.directives.string());
  {
auto checked261 = legs.instanceValue("humanoidAnimationTags", JsonObject());
if (!checked261) return std::unexpected(std::move(checked261.error()));
auto checked262 = std::move(*checked261).iterateObject();
if (!checked262) return std::unexpected(std::move(checked262.error()));
for (auto tag : std::move(*checked262)) {
    auto checked263 = tag.first.replace("<slot>", toString(slot + 1));
if (!checked263) return std::unexpected(std::move(checked263.error()));
auto checked264 = m_networkedAnimator->applyPartTags(m_legsArmorOffsetPoint.first, std::move(*checked263));
if (!checked264) return std::unexpected(std::move(checked264.error()));
auto checked265 = tag.second.toString();
if (!checked265) return std::unexpected(std::move(checked265.error()));
auto checked266 = m_networkedAnimator->applyPartTags(m_legsArmorOffsetPoint.first, std::move(*checked265));
if (!checked266) return std::unexpected(std::move(checked266.error()));
wornLegs.animationTags.set(
      std::move(*checked264),
      std::move(*checked266)
    );
  }
}
  return {};
}

std::expected<void, StarException> Humanoid::setWearableFromBack(std::uint8_t slot, BackArmor const& back, Gender gender) {
  auto& fashion = *m_fashion;
  auto checked267 = fashion.wearables.at(slot);
if (!checked267) return std::unexpected(std::move(checked267.error()));
Wearable& current = checked267->get();
  if (!std::holds_alternative<WornBack>(current)) {
    wearableRemoved(current);
    fashion.wornBacksChanged = true;
  }

  auto& wornBack = current.emplace<WornBack>();
  wornBack.directives = back.directives(m_facingDirection == Direction::Left);
  wornBack.fullbright = back.fullbright();
  wornBack.frameset = back.frameset(gender);
  auto checked268 = back.instanceValue("rotateWithHead", false);
if (!checked268) return std::unexpected(std::move(checked268.error()));
auto checked269 = std::move(*checked268).optBool();
if (!checked269) return std::unexpected(std::move(checked269.error()));
wornBack.rotateWithHead = std::move(*checked269).value_or(false);
  wornBack.animationTags.clear();
  wornBack.animationTags.set(strf("backCosmetic{}Frameset", slot+1), wornBack.frameset);
  wornBack.animationTags.set(strf("backCosmetic{}Directives", slot+1), wornBack.directives.string());
  {
auto checked270 = back.instanceValue("humanoidAnimationTags", JsonObject());
if (!checked270) return std::unexpected(std::move(checked270.error()));
auto checked271 = std::move(*checked270).iterateObject();
if (!checked271) return std::unexpected(std::move(checked271.error()));
for (auto tag : std::move(*checked271)) {
    auto checked272 = tag.first.replace("<slot>", toString(slot + 1));
if (!checked272) return std::unexpected(std::move(checked272.error()));
auto checked273 = m_networkedAnimator->applyPartTags(m_backArmorOffsetPoint.first, std::move(*checked272));
if (!checked273) return std::unexpected(std::move(checked273.error()));
auto checked274 = tag.second.toString();
if (!checked274) return std::unexpected(std::move(checked274.error()));
auto checked275 = m_networkedAnimator->applyPartTags(m_backArmorOffsetPoint.first, std::move(*checked274));
if (!checked275) return std::unexpected(std::move(checked275.error()));
wornBack.animationTags.set(
      std::move(*checked273),
      std::move(*checked275)
    );
  }
}
  return {};
}

const std::uint8_t ChestLegsSortOrder[21] = {255, 14, 8, 2, 0, 15, 9, 3, 1, 4, 5, 6, 7, 10, 11, 12, 13, 16, 17, 18, 19};

void Humanoid::refreshWearables(Fashion& fashion) {
  bool wornHeadsChanged = fashion.wornHeadsChanged;
  bool wornChestsLegsChanged = fashion.wornChestsLegsChanged;
  bool wornBacksChanged = fashion.wornBacksChanged;
  bool helmetMasksChanged = fashion.helmetMasksChanged;
  if (!wornHeadsChanged && !wornChestsLegsChanged && !wornBacksChanged && !helmetMasksChanged)
    return;

  if (wornHeadsChanged)
    fashion.wornHeads.fill(0);
  if (wornChestsLegsChanged)
    fashion.wornChestsLegs.fill(0);
  if (wornBacksChanged)
    fashion.wornBacks.fill(0);
  if (helmetMasksChanged)
    fashion.helmetMaskDirectivesGroup.clear();

  auto setTags = [&](HashMap<String,String> tags) {
    if (m_useAnimation)
      for (auto tag : tags) {
        m_networkedAnimator->setLocalTag(tag.first, tag.second);
      }
  };

  std::uint8_t headI = 0, chestsLegsI = 0, backsI = 0;
  for (std::uint8_t i = 0; i != fashion.wearables.size(); ++i) {
    auto& wearable = fashion.wearables[i];
    if (std::holds_alternative<std::monostate>(wearable))
      continue;
    else if (std::holds_alternative<WornHead>(wearable)) {
      auto const& head = std::get<WornHead>(wearable);
      if (helmetMasksChanged)
        fashion.helmetMaskDirectivesGroup += head.maskDirectives;
      if (wornHeadsChanged) {
        fashion.wornHeads[headI++] = i + 1;
        setTags(head.animationTags);
      }
    } else if (std::holds_alternative<WornChest>(wearable) || std::holds_alternative<WornLegs>(wearable)) {
      if (wornChestsLegsChanged) {
        fashion.wornChestsLegs[chestsLegsI++] = i + 1;
        if (std::holds_alternative<WornChest>(wearable))
          setTags(std::get<WornChest>(wearable).animationTags);
        else
          setTags(std::get<WornLegs>(wearable).animationTags);
      }

    } else if (std::holds_alternative<WornBack>(wearable)) {
      if (wornBacksChanged) {
        fashion.wornBacks[backsI++] = i + 1;
        setTags(std::get<WornBack>(wearable).animationTags);
      }
    }
  }
  if (wornChestsLegsChanged) {
    std::sort(fashion.wornChestsLegs.begin(), fashion.wornChestsLegs.end(), [](std::uint8_t a, std::uint8_t b) {
      return ChestLegsSortOrder[a] < ChestLegsSortOrder[b];
    });
  }

  if (m_useAnimation)
    {
m_networkedAnimator->setLocalTag("helmetMaskDirectives", fashion.helmetMaskDirectivesGroup.toString());
}

  fashion.wornHeadsChanged = fashion.wornChestsLegsChanged = fashion.wornBacksChanged = fashion.helmetMasksChanged = false;
}

const Directives nullDirectives = {};
const String nullFrameset = "";

Directives const& Humanoid::headArmorDirectives() const {
  if (auto head = getLastWearableOfType<WornHead>())
    return head->get().directives;
  return nullDirectives;
};

String const& Humanoid::headArmorFrameset() const {
  if (auto head = getLastWearableOfType<WornHead>())
    return head->get().frameset;
  return nullFrameset;
};

Directives const& Humanoid::chestArmorDirectives() const {
  if (auto chest = getLastWearableOfType<WornChest>())
    return chest->get().directives;
  return nullDirectives;
};

String const& Humanoid::chestArmorFrameset() const {
  if (auto chest = getLastWearableOfType<WornChest>())
    return chest->get().frameset;
  return nullFrameset;
};

String const& Humanoid::backSleeveFrameset() const {
  if (auto chest = getLastWearableOfType<WornChest>())
    return chest->get().backSleeveFrameset;
  return nullFrameset;
};

String const& Humanoid::frontSleeveFrameset() const {
  if (auto chest = getLastWearableOfType<WornChest>())
    return chest->get().frontSleeveFrameset;
  return nullFrameset;
};

Directives const& Humanoid::legsArmorDirectives() const {
  if (auto legs = getLastWearableOfType<WornLegs>())
    return legs->get().directives;
  return nullDirectives;
};

String const& Humanoid::legsArmorFrameset() const {
  if (auto legs = getLastWearableOfType<WornLegs>())
    return legs->get().frameset;
  return nullFrameset;
};

Directives const& Humanoid::backArmorDirectives() const {
  if (auto back = getLastWearableOfType<WornBack>())
    return back->get().directives;
  return nullDirectives;
};

String const& Humanoid::backArmorFrameset() const {
  if (auto back = getLastWearableOfType<WornBack>())
    return back->get().frameset;
  return nullFrameset;
};

void Humanoid::setBodyHidden(bool hidden) {
  m_bodyHidden = hidden;
}

std::expected<void, StarException> Humanoid::setState(State state) {
  if (m_state != state) {
    m_state = state;
    m_animationTimer = 0.0f;
    m_previousAnimationTimer = 0;
    if (m_useAnimation)
      {
auto checked277 = refreshAnimationState();
if (!checked277) return std::unexpected(std::move(checked277.error()));
}

  }
  return {};
}

std::expected<void, StarException> Humanoid::setEmoteState(HumanoidEmote state) {
  if (m_emoteState != state) {
    m_emoteState = state;
    m_emoteAnimationTimer = 0.0f;
    m_previousEmoteAnimationTimer = 0;
    if (m_useAnimation) {
      if (auto animationStates = m_emoteAnimationStates.maybe(m_emoteState))
        for (auto args : *animationStates)
          {
auto checked278 = m_networkedAnimator->setLocalState(args.first, args.second.state, args.second.startNew, args.second.reverse);
if (!checked278) return std::unexpected(std::move(checked278.error()));
}
    }
  }
  return {};
}

std::expected<void, StarException> Humanoid::setDance(std::optional<String> const& dance) {
  if (m_dance != dance) {
    m_danceTimer = 0.0f;
    m_previousDanceTimer = 0;
    bool checked280 = m_useAnimation && dance.has_value();
if (checked280) {
auto checked279 = m_networkedAnimator->hasState("dance", dance.value_or(String{}));
if (!checked279) return std::unexpected(std::move(checked279.error()));
checked280 = std::move(*checked279);
}
if (checked280)
      {
auto checked281 = m_networkedAnimator->setLocalState("dance", dance.value_or(String{}));
if (!checked281) return std::unexpected(std::move(checked281.error()));
}
  }
  m_dance = dance;
  return {};
}

void Humanoid::setFacingDirection(Direction facingDirection) {
  m_facingDirection = facingDirection;
  if (m_useAnimation)
    m_networkedAnimator->setFlipped(m_facingDirection == Direction::Left);
}

std::expected<void, StarException> Humanoid::setMovingBackwards(bool movingBackwards) {
  if (m_movingBackwards != movingBackwards) {
    m_movingBackwards = movingBackwards;
    if (m_useAnimation)
      {
auto checked282 = refreshAnimationState();
if (!checked282) return std::unexpected(std::move(checked282.error()));
}
  }
  return {};
}

void Humanoid::setHeadRotation(float headRotation) {
  m_headRotationTarget = headRotation;
}

void Humanoid::setRotation(float rotation) {
  m_rotation = rotation;
}

void Humanoid::setScale(Vec2F scale) {
  m_scale = scale;
}

void Humanoid::setVaporTrail(bool enabled) {
  m_drawVaporTrail = enabled;
}

Humanoid::State Humanoid::state() const {
  return m_state;
}

HumanoidEmote Humanoid::emoteState() const {
  return m_emoteState;
}

std::optional<String> Humanoid::dance() const {
  return m_dance;
}

std::expected<bool, StarException> Humanoid::danceCyclicOrEnded() const {
  if (!m_dance)
    return false;

  auto checked283 = m_root.danceDatabase();
if (!checked283) return std::unexpected(std::move(checked283.error()));
auto danceDatabase = std::move(*checked283);
  auto checked284 = danceDatabase->getDance(*m_dance);
if (!checked284) return std::unexpected(std::move(checked284.error()));
auto dance = std::move(*checked284);
  return dance->cyclic || m_danceTimer > dance->duration;
}

Direction Humanoid::facingDirection() const {
  return m_facingDirection;
}

bool Humanoid::movingBackwards() const {
  return m_movingBackwards;
}

void Humanoid::setHandParameters(ToolHand hand, bool holdingItem, float angle, float itemAngle, bool twoHanded,
    bool recoil, bool outsideOfHand) {
  auto& handInfo = const_cast<HandDrawingInfo&>(getHand(hand));
  handInfo.holdingItem = holdingItem;
  handInfo.angle = angle;
  handInfo.itemAngle = itemAngle;
  handInfo.recoil = recoil;
  handInfo.outsideOfHand = outsideOfHand;
  if (hand == ToolHand::Primary)
    m_twoHanded = twoHanded;
}

void Humanoid::setHandFrameOverrides(ToolHand hand, StringView back, StringView front) {
  auto& handInfo = const_cast<HandDrawingInfo&>(getHand(hand));
  // some users stick directives in these?? better make sure they don't break with custom clothing
  std::size_t  backEnd =  back.utf8().find('?');
  std::size_t frontEnd = front.utf8().find('?');
  Directives  backDirectives =  backEnd == NPos ? Directives() : Directives( back.utf8().substr( backEnd));
  Directives frontDirectives = frontEnd == NPos ? Directives() : Directives(front.utf8().substr(frontEnd));
  if ( backEnd != NPos)  back =  back.utf8().substr(0,  backEnd);
  if (frontEnd != NPos) front = front.utf8().substr(0, frontEnd);
  handInfo. backFrame = ! back.empty() ? std::move( back) : "rotation";
  handInfo.frontFrame = !front.empty() ? std::move(front) : "rotation";
  handInfo. backDirectives = ! backDirectives.empty() ? std::move( backDirectives) : Directives();
  handInfo.frontDirectives = !frontDirectives.empty() ? std::move(frontDirectives) : Directives();
}

void Humanoid::setHandDrawables(ToolHand hand, List<Drawable> drawables) {
  const_cast<HandDrawingInfo&>(getHand(hand)).itemDrawables = std::move(drawables);
}

void Humanoid::setHandNonRotatedDrawables(ToolHand hand, List<Drawable> drawables) {
  const_cast<HandDrawingInfo&>(getHand(hand)).nonRotatedDrawables = std::move(drawables);
}

bool Humanoid::handHoldingItem(ToolHand hand) const {
  return getHand(hand).holdingItem;
}

std::expected<void, StarException> Humanoid::animate(float dt, std::optional<std::reference_wrapper<NetworkedAnimator::DynamicTarget>> dynamicTarget) {
  m_previousAnimationTimer = m_animationTimer;
  m_previousEmoteAnimationTimer = m_emoteAnimationTimer;
  m_previousDanceTimer = m_danceTimer;
  m_previousPose = m_currentPose;
  m_animationTimer += dt;
  m_emoteAnimationTimer += dt;
  m_danceTimer += dt;
  auto headRotationEnabled = m_root.humanoidHeadRotation();
  if (!headRotationEnabled) return std::unexpected(std::move(headRotationEnabled.error()));
  float headRotationTarget = headRotationEnabled->get() ? m_headRotationTarget : 0.f;
  m_headRotation = (headRotationTarget - (headRotationTarget - m_headRotation) * std::powf(.333333f, dt * 60.f));
  m_currentPose = PresentationPose{m_headRotation, m_rotation, m_scale,
      m_primaryHand.angle, m_primaryHand.itemAngle, m_altHand.angle, m_altHand.itemAngle,
      m_primaryHand.holdingItem, m_altHand.holdingItem, m_facingDirection, m_twoHanded};
  if (!m_presentationHistory || m_previousPose.facing != m_currentPose.facing || m_previousPose.twoHanded != m_currentPose.twoHanded)
    m_previousPose = m_currentPose;
  m_presentationHistory = true;
  auto posed = prepareAnimatorPose();
  if (!posed) return std::unexpected(std::move(posed.error()));

  auto checked285 = m_networkedAnimator->update(dt, dynamicTarget);
if (!checked285) return std::unexpected(std::move(checked285.error()));
  return {};
}

std::expected<void, StarException> Humanoid::resetAnimation() {
  m_animationTimer = 0.0f;
  m_emoteAnimationTimer = 0.0f;
  m_danceTimer = 0.0f;
  m_headRotation = m_root.humanoidHeadRotation() ? 0.f : m_headRotationTarget;
  m_previousAnimationTimer = m_previousEmoteAnimationTimer = m_previousDanceTimer = 0;
  m_presentationHistory = false;

  if (m_useAnimation) {
    auto checked286 = m_networkedAnimator->finishAnimations();
if (!checked286) return std::unexpected(std::move(checked286.error()));
    // reset set all animations and force startnew
    if (auto animationStates = m_emoteAnimationStates.maybe(m_emoteState))
      for (auto args : *animationStates)
        {
auto checked287 = m_networkedAnimator->setLocalState(args.first, args.second.state, true, args.second.reverse);
if (!checked287) return std::unexpected(std::move(checked287.error()));
}
    auto checked288 = refreshAnimationState(true);
if (!checked288) return std::unexpected(std::move(checked288.error()));
  }
  return prepareAnimatorPose();
}

template <typename Animator>
std::expected<void, StarException> Humanoid::applyAnimatorPose(Animator& animator, HandDrawingInfo const& frontHand, HandDrawingInfo const& backHand,
    float frontAngle, float backAngle, float headRotation, DanceStep const* danceStep, bool withItems) {
  auto& fashion = *m_fashion;
    auto checked293 = animator.resetLocalTransformationGroup("headRotation");
if (!checked293) return std::unexpected(std::move(checked293.error()));
    auto checked294 = animator.resetLocalTransformationGroup("bodyHeadRotation");
if (!checked294) return std::unexpected(std::move(checked294.error()));
    for (std::uint8_t i : fashion.wornBacks) {
      if (i == 0)
        break;
      auto checked295 = animator.resetLocalTransformationGroup("backCosmetic" + toString(i) + "Rotation");
if (!checked295) return std::unexpected(std::move(checked295.error()));
    }
    if (headRotation != 0.f) {
      float dir = numericalDirection(m_facingDirection);
      float headX = (headRotation / ((float)std::numbers::pi * 2.f));
      Vec2F translate = {
        -(state() == State::Run ? (std::fmaxf(headX, 0.f) * 2.f) : headX),
        -(std::fabsf(headRotation / ((float)std::numbers::pi * 4.f)))
      };
      auto checked296 = animator.partProperty(m_headRotationPoint.first, m_headRotationPoint.second);
if (!checked296) return std::unexpected(std::move(checked296.error()));
auto checked297 = jsonToVec2F(std::move(*checked296));
if (!checked297) return std::unexpected(std::move(checked297.error()));
auto rotationCenter = std::move(*checked297);
      auto checked298 = animator.partTransformation(m_headRotationPoint.first);
if (!checked298) return std::unexpected(std::move(checked298.error()));
auto bodyHeadRotationCenter = std::move(*checked298).transformVec2(rotationCenter);
      auto checked299 = animator.rotateLocalTransformationGroup("headRotation", headRotation * dir, rotationCenter);
if (!checked299) return std::unexpected(std::move(checked299.error()));
      auto checked300 = animator.translateLocalTransformationGroup("headRotation", translate);
if (!checked300) return std::unexpected(std::move(checked300.error()));
      auto checked301 = animator.rotateLocalTransformationGroup("bodyHeadRotation", headRotation * dir, rotationCenter);
if (!checked301) return std::unexpected(std::move(checked301.error()));
      auto checked302 = animator.translateLocalTransformationGroup("bodyHeadRotation", translate);
if (!checked302) return std::unexpected(std::move(checked302.error()));
      for (std::uint8_t i : fashion.wornBacks) {
        if (i == 0)
          break;
        auto& back = std::get<WornBack>(fashion.wearables[std::size_t(i) - 1]);
        if (back.rotateWithHead) {
          auto checked303 = animator.rotateLocalTransformationGroup("backCosmetic" + toString(i) + "Rotation", headRotation * dir, bodyHeadRotationCenter);
if (!checked303) return std::unexpected(std::move(checked303.error()));
          auto checked304 = animator.translateLocalTransformationGroup("backCosmetic" + toString(i) + "Rotation", translate);
if (!checked304) return std::unexpected(std::move(checked304.error()));
        }
      }
    }

    auto checked305 = animator.resetLocalTransformationGroup("backArmRotation");
if (!checked305) return std::unexpected(std::move(checked305.error()));
    auto checked306 = animator.resetLocalTransformationGroup("frontArmRotation");
if (!checked306) return std::unexpected(std::move(checked306.error()));

    if (danceStep) {

      if (danceStep->bodyFrame.has_value()) {
        auto danceFrame = danceStep->bodyFrame.value_or(String{});
        animator.setLocalTag("bodyDanceFrame", danceFrame);
        auto checked307 = animator.hasState("bodyDance", danceFrame);
if (!checked307) return std::unexpected(std::move(checked307.error()));
auto checked308 = animator.setLocalState("bodyDance", std::move(*checked307) ? danceFrame : "dance");
if (!checked308) return std::unexpected(std::move(checked308.error()));
      } else {
        auto checked309 = animator.setLocalState("bodyDance", "idle");
if (!checked309) return std::unexpected(std::move(checked309.error()));
        animator.setLocalTag("bodyDanceFrame");
      }

      auto checked310 = animator.translateLocalTransformationGroup("backArmRotation", danceStep->backArmOffset / TilePixels);
if (!checked310) return std::unexpected(std::move(checked310.error()));
      auto checked311 = animator.rotateLocalTransformationGroup("backArmRotation", danceStep->backArmRotation);
if (!checked311) return std::unexpected(std::move(checked311.error()));
      if (danceStep->backArmFrame.has_value()) {
        auto danceFrame = danceStep->backArmFrame.value_or(String{});
        animator.setLocalTag("backArmDanceFrame", danceFrame);
        auto checked312 = animator.hasState("backArmDance", danceFrame);
if (!checked312) return std::unexpected(std::move(checked312.error()));
auto checked313 = animator.setLocalState("backArmDance", std::move(*checked312) ? danceFrame : "dance");
if (!checked313) return std::unexpected(std::move(checked313.error()));
      } else {
        auto checked314 = animator.setLocalState("backArmDance", "idle");
if (!checked314) return std::unexpected(std::move(checked314.error()));
        animator.setLocalTag("backArmDanceFrame");
      }
      auto checked315 = animator.setLocalState("backArm", "idle");
if (!checked315) return std::unexpected(std::move(checked315.error()));

      auto checked316 = animator.translateLocalTransformationGroup("frontArmRotation", danceStep->frontArmOffset / TilePixels);
if (!checked316) return std::unexpected(std::move(checked316.error()));
      auto checked317 = animator.rotateLocalTransformationGroup("frontArmRotation", danceStep->frontArmRotation);
if (!checked317) return std::unexpected(std::move(checked317.error()));
      if (danceStep->frontArmFrame.has_value()) {
        auto danceFrame = danceStep->frontArmFrame.value_or(String{});
        animator.setLocalTag("frontArmDanceFrame", danceFrame);
        auto checked318 = animator.hasState("frontArmDance", danceFrame);
if (!checked318) return std::unexpected(std::move(checked318.error()));
auto checked319 = animator.setLocalState("frontArmDance", std::move(*checked318) ? danceFrame : "dance");
if (!checked319) return std::unexpected(std::move(checked319.error()));
      } else {
        auto checked320 = animator.setLocalState("frontArmDance", "idle");
if (!checked320) return std::unexpected(std::move(checked320.error()));
        animator.setLocalTag("frontArmDanceFrame");
      }
      auto checked321 = animator.setLocalState("frontArm", "idle");
if (!checked321) return std::unexpected(std::move(checked321.error()));

    } else {
      auto checked322 = animator.setLocalState("bodyDance", "idle");
if (!checked322) return std::unexpected(std::move(checked322.error()));
      animator.setLocalTag("bodyDanceFrame");

      auto checked323 = animator.setLocalState("backArmDance", "idle");
if (!checked323) return std::unexpected(std::move(checked323.error()));
      animator.setLocalTag("backArmDanceFrame");
      auto checked324 = animator.partProperty(m_backArmRotationPoint.first, m_backArmRotationPoint.second);
if (!checked324) return std::unexpected(std::move(checked324.error()));
auto checked325 = jsonToVec2F(std::move(*checked324));
if (!checked325) return std::unexpected(std::move(checked325.error()));
auto checked326 = animator.rotateLocalTransformationGroup("backArmRotation",
        backAngle,
        std::move(*checked325)
      );
if (!checked326) return std::unexpected(std::move(checked326.error()));
      if (backHand.recoil)
        {
auto checked327 = animator.translateLocalTransformationGroup("backArmRotation", m_recoilOffset);
if (!checked327) return std::unexpected(std::move(checked327.error()));
}
      if (backHand.holdingItem && withItems) {
        animator.setLocalTag("backArmFrame", backHand.backFrame);
        auto checked328 = animator.hasState("backArm", backHand.backFrame);
if (!checked328) return std::unexpected(std::move(checked328.error()));
auto checked329 = animator.setLocalState("backArm", std::move(*checked328) ? backHand.backFrame : "rotation");
if (!checked329) return std::unexpected(std::move(checked329.error()));
        auto checked330 = animator.setLocalState("backHandItem", backHand.outsideOfHand ? String("outside") : String("inside"));
if (!checked330) return std::unexpected(std::move(checked330.error()));
      } else {
        auto checked331 = animator.setLocalState("backArm", "idle");
if (!checked331) return std::unexpected(std::move(checked331.error()));
      }

      auto checked332 = animator.setLocalState("frontArmDance", "idle");
if (!checked332) return std::unexpected(std::move(checked332.error()));
      animator.setLocalTag("frontArmDanceFrame");
      auto checked333 = animator.partProperty(m_frontArmRotationPoint.first, m_frontArmRotationPoint.second);
if (!checked333) return std::unexpected(std::move(checked333.error()));
auto checked334 = jsonToVec2F(std::move(*checked333));
if (!checked334) return std::unexpected(std::move(checked334.error()));
auto checked335 = animator.rotateLocalTransformationGroup("frontArmRotation",
        frontAngle,
        std::move(*checked334)
      );
if (!checked335) return std::unexpected(std::move(checked335.error()));
      if (frontHand.recoil)
        {
auto checked336 = animator.translateLocalTransformationGroup("frontArmRotation", m_recoilOffset);
if (!checked336) return std::unexpected(std::move(checked336.error()));
}
      if (frontHand.holdingItem && withItems) {
        animator.setLocalTag("frontArmFrame", frontHand.frontFrame);
        auto checked337 = animator.hasState("frontArm", frontHand.frontFrame);
if (!checked337) return std::unexpected(std::move(checked337.error()));
auto checked338 = animator.setLocalState("frontArm", std::move(*checked337) ? frontHand.frontFrame : "rotation");
if (!checked338) return std::unexpected(std::move(checked338.error()));

        auto checked339 = animator.setLocalState("frontHandItem", frontHand.outsideOfHand ? String("outside") : String("inside"));
if (!checked339) return std::unexpected(std::move(checked339.error()));
      } else {
        auto checked340 = animator.setLocalState("frontArm", "idle");
if (!checked340) return std::unexpected(std::move(checked340.error()));
      }
    }
  return {};
}

std::expected<void, StarException> Humanoid::prepareAnimatorPose() {
  return prepareAnimatorPose(*m_networkedAnimator, 1.f, true);
}

std::expected<void, StarException> Humanoid::prepareAnimatorPose(NetworkedAnimator::Sample& sample, bool withItems) {
  return prepareAnimatorPose(sample, sample.interpolation(), withItems);
}

template <typename Animator>
std::expected<void, StarException> Humanoid::prepareAnimatorPose(Animator& animator, float interpolation, bool withItems) {
  if (!m_useAnimation)
    return {};
  interpolation = std::clamp(interpolation, 0.f, 1.f);
  refreshWearables(*m_fashion);
  auto dance = getDance();
  if (!dance) return std::unexpected(std::move(dance.error()));
  DanceStep const* danceStep = nullptr;
  if (*dance) {
    if (!StateNames) return std::unexpected(StateNames.error());
    auto name = StateNames->getRight(m_state);
    if (!name) return std::unexpected(std::move(name.error()));
    if ((**dance)->states.contains(name->get()))
      danceStep = &(**dance)->steps[m_timing.danceSeq(lerp(interpolation, m_previousDanceTimer, m_danceTimer), **dance)];
  }
  bool frontPrimary = m_facingDirection == Direction::Left || m_twoHanded;
  bool backPrimary = m_facingDirection == Direction::Right || m_twoHanded;
  auto const& front = frontPrimary ? m_primaryHand : m_altHand;
  auto const& back = backPrimary ? m_primaryHand : m_altHand;
  auto angle = [&](HandDrawingInfo const& hand, bool primary) {
    if (!m_presentationHistory || m_previousPose.facing != m_facingDirection || m_previousPose.twoHanded != m_twoHanded
        || hand.holdingItem != (primary ? m_previousPose.primaryHolding : m_previousPose.altHolding))
      return hand.angle;
    return angleLerp(interpolation, primary ? m_previousPose.primaryAngle : m_previousPose.altAngle, hand.angle);
  };
  float head = m_presentationHistory ? angleLerp(interpolation, m_previousPose.headRotation, m_headRotation) : m_headRotation;
  return applyAnimatorPose(animator, front, back, angle(front, frontPrimary), angle(back, backPrimary), head, danceStep, withItems);
}

std::expected<List<Drawable>, StarException> Humanoid::render(bool withItems, bool withRotationAndScale, float interpolation) {
  auto sample = m_networkedAnimator->sample(interpolation);
  return render(sample, withItems, withRotationAndScale);
}

std::expected<List<Drawable>, StarException> Humanoid::render(NetworkedAnimator::Sample& sample, bool withItems, bool withRotationAndScale) {
  float interpolation = sample.interpolation();
  float headRotation = m_presentationHistory ? angleLerp(interpolation, m_previousPose.headRotation, m_headRotation) : m_headRotation;
  float rotation = m_presentationHistory ? angleLerp(interpolation, m_previousPose.rotation, m_rotation) : m_rotation;
  Vec2F scale = m_presentationHistory ? lerp(interpolation, m_previousPose.scale, m_scale) : m_scale;
  auto& fashion = *m_fashion;
  refreshWearables(fashion);
  List<Drawable> drawables;

  auto checked289 = getArmStateSequence(interpolation);
if (!checked289) return std::unexpected(std::move(checked289.error()));
int armStateSeq = std::move(*checked289);
  int bodyStateSeq = getBodyStateSequence(interpolation);
  int emoteStateSeq = getEmoteStateSequence(interpolation);
  auto checked290 = getBobYOffset(interpolation);
if (!checked290) return std::unexpected(std::move(checked290.error()));
float bobYOffset = std::move(*checked290);
  auto checked291 = getDance();
if (!checked291) return std::unexpected(std::move(checked291.error()));
std::optional<std::shared_ptr<Dance>> dance = std::move(*checked291);
  std::optional<DanceStep> danceStep = {};
  if (dance.has_value()) {
    if (!StateNames) return std::unexpected(StateNames.error());
auto checked292 = StateNames->getRight(m_state);
if (!checked292) return std::unexpected(std::move(checked292.error()));
if (!(*dance)->states.contains(checked292->get()))
      dance = {};
    else
      danceStep = (*dance)->steps[m_timing.danceSeq(lerp(interpolation, m_previousDanceTimer, m_danceTimer), *dance)];
  }

  auto const& frontHand = (m_facingDirection == Direction::Left || m_twoHanded) ? m_primaryHand : m_altHand;
  auto const& backHand = (m_facingDirection == Direction::Right || m_twoHanded) ? m_primaryHand : m_altHand;
  auto sampleAngle = [&](HandDrawingInfo const& hand, bool primary, bool item) {
    float angle = item ? hand.itemAngle : hand.angle;
    if (!m_presentationHistory || m_previousPose.facing != m_facingDirection || m_previousPose.twoHanded != m_twoHanded
        || hand.holdingItem != (primary ? m_previousPose.primaryHolding : m_previousPose.altHolding))
      return angle;
    float previous = item ? (primary ? m_previousPose.primaryItemAngle : m_previousPose.altItemAngle)
                          : (primary ? m_previousPose.primaryAngle : m_previousPose.altAngle);
    return angleLerp(interpolation, previous, angle);
  };
  bool frontPrimary = m_facingDirection == Direction::Left || m_twoHanded;
  bool backPrimary = m_facingDirection == Direction::Right || m_twoHanded;
  float frontAngle = sampleAngle(frontHand, frontPrimary, false);
  float backAngle = sampleAngle(backHand, backPrimary, false);
  float frontItemAngle = sampleAngle(frontHand, frontPrimary, true);
  float backItemAngle = sampleAngle(backHand, backPrimary, true);

  Vec2F frontArmFrameOffset = Vec2F(0, bobYOffset);
  if (frontHand.recoil)
    frontArmFrameOffset += m_recoilOffset;
  Vec2F backArmFrameOffset = Vec2F(0, bobYOffset);
  if (backHand.recoil)
    backArmFrameOffset += m_recoilOffset;

  if (m_useAnimation) {
    auto posed = applyAnimatorPose(sample, frontHand, backHand, frontAngle, backAngle, headRotation,
        danceStep ? &*danceStep : nullptr, withItems);
    if (!posed) return std::unexpected(std::move(posed.error()));

    sample.setPartDrawables(m_backItemPart, !danceStep && withItems && backHand.holdingItem && !m_twoHanded ? &backHand.itemDrawables : nullptr);
    sample.setPartDrawables(m_frontItemPart, !danceStep && withItems && frontHand.holdingItem ? &frontHand.itemDrawables : nullptr);
    auto checked341 = sample.drawables();
if (!checked341) return std::unexpected(std::move(checked341.error()));
auto animatorDrawables = std::move(*checked341);
    if (withRotationAndScale) {
      Drawable::rotateAll(animatorDrawables, rotation);
      Drawable::scaleAll(animatorDrawables, scale);
    }

    if (withItems) {
      if (m_altHand.nonRotatedDrawables.size())
        drawables.appendAll(m_altHand.nonRotatedDrawables);
      if (m_primaryHand.nonRotatedDrawables.size())
        drawables.appendAll(m_primaryHand.nonRotatedDrawables);
    }
    drawables.appendAll(std::move(animatorDrawables));
    Drawable::rebaseAll(drawables);
  } else {
    auto addDrawable = [&](Drawable drawable, bool forceFullbright = false) -> Drawable& {
      if (m_facingDirection == Direction::Left)
        drawable.scale(Vec2F(-1, 1));
      drawable.fullbright |= forceFullbright;
      drawables.append(std::move(drawable));
      return drawables[drawables.size() - 1];
    };

    auto backArmDrawable = [&](String const& frameSet, Directives const& directives) -> std::expected<Drawable, StarException> {
      String image = strf("{}:{}{}", frameSet, backHand.backFrame, directives.prefix());
      auto checked342 = m_root.imageMetadataDatabase();
if (!checked342) return std::unexpected(std::move(checked342.error()));
auto checked343 = Drawable::makeImage(*std::move(*checked342), std::move(image), 1.0f / TilePixels, true, backArmFrameOffset);
if (!checked343) return std::unexpected(std::move(checked343.error()));
Drawable backArm = std::move(*checked343);
      auto checked344 = m_root.imageMetadataDatabase();
if (!checked344) return std::unexpected(std::move(checked344.error()));
auto checked345 = backArm.imagePart().addDirectives(*std::move(*checked344), directives, true);
if (!checked345) return std::unexpected(std::move(checked345.error()));
      auto checked346 = m_root.imageMetadataDatabase();
if (!checked346) return std::unexpected(std::move(checked346.error()));
auto checked347 = backArm.imagePart().addDirectives(*std::move(*checked346), backHand.backDirectives, true);
if (!checked347) return std::unexpected(std::move(checked347.error()));
      backArm.rotate(backAngle, backArmFrameOffset + m_backArmRotationCenter + m_backArmOffset);
      return backArm;
    };

    Vec2F headPosition(0, bobYOffset);
    if (dance.has_value())
      headPosition += danceStep->headOffset / TilePixels;
    else if (m_state == Idle)
      headPosition += m_identity.personality.headOffset / TilePixels;
    else if (m_state == Run)
      headPosition += m_headRunOffset;
    else if (m_state == Swim || m_state == SwimIdle)
      headPosition += m_headSwimOffset;
    else if (m_state == Duck)
      headPosition += m_headDuckOffset;
    else if (m_state == Sit)
      headPosition += m_headSitOffset;
    else if (m_state == Lay)
      headPosition += m_headLayOffset;

    auto applyHeadRotation = [&](Drawable& drawable) {
      if (headRotation != 0.f && withRotationAndScale) {
        float dir = numericalDirection(m_facingDirection);
        Vec2F rotationPoint = headPosition + m_headRotationCenter;
        rotationPoint[0] *= dir;
        float headX = (headRotation / ((float)std::numbers::pi * 2.f));
        drawable.rotate(headRotation, rotationPoint);
        drawable.position[0] -= state() == State::Run ? (std::fmaxf(headX * dir, 0.f) * 2.f) * dir : headX;
        drawable.position[1] -= std::fabsf(headRotation / ((float)std::numbers::pi * 4.f));
      }
    };

    for (std::uint8_t i : fashion.wornBacks) {
      if (i == 0)
        break;
      auto& back = std::get<WornBack>(fashion.wearables[std::size_t(i) - 1]);
      if (!back.frameset.empty()) {
        auto checked348 = frameBase(m_state);
if (!checked348) return std::unexpected(std::move(checked348.error()));
auto frameGroup = std::move(*checked348);
        auto prefix = back.directives.prefix();
        if (m_movingBackwards && (m_state == State::Run))
          frameGroup = "runbackwards";
        String image;
        if (dance.has_value() && danceStep->bodyFrame)
          image = strf("{}:{}{}", back.frameset, *danceStep->bodyFrame, prefix);
        else if (m_state == Idle)
          image = strf("{}:{}{}", back.frameset, m_identity.personality.idle, prefix);
        else
          image = strf("{}:{}.{}{}", back.frameset, frameGroup, bodyStateSeq, prefix);

        auto checked349 = m_root.imageMetadataDatabase();
if (!checked349) return std::unexpected(std::move(checked349.error()));
auto checked350 = Drawable::makeImage(*std::move(*checked349), std::move(image), 1.0f / TilePixels, true, Vec2F());
if (!checked350) return std::unexpected(std::move(checked350.error()));
auto drawable = std::move(*checked350);
        auto checked351 = m_root.imageMetadataDatabase();
if (!checked351) return std::unexpected(std::move(checked351.error()));
auto checked352 = drawable.imagePart().addDirectives(*std::move(*checked351), back.directives, true);
if (!checked352) return std::unexpected(std::move(checked352.error()));
        Drawable& applied = addDrawable(std::move(drawable), back.fullbright);
        if (back.rotateWithHead)
          applyHeadRotation(applied);
      }
    }

    auto drawBackArmAndSleeves = [&](bool holdingItem) -> std::expected<void, StarException> {
      auto bodyDirectives = getBodyDirectives();
      if (holdingItem && !m_bodyHidden)
        {
auto checked353 = backArmDrawable(m_backArmFrameset, bodyDirectives);
if (!checked353) return std::unexpected(std::move(checked353.error()));
addDrawable(std::move(*checked353), m_bodyFullbright);
}
      else if (!m_backArmFrameset.empty() && !m_bodyHidden) {
        String image;
        Vec2F position;
        auto prefix = bodyDirectives.prefix();
        if (dance.has_value() && danceStep->backArmFrame) {
          image = strf("{}:{}{}", m_backArmFrameset, *danceStep->backArmFrame, prefix);
          position = danceStep->backArmOffset / TilePixels;
        } else if (m_state == Idle) {
          image = strf("{}:{}{}", m_backArmFrameset, m_identity.personality.armIdle, prefix);
          position = m_identity.personality.armOffset / TilePixels;
        } else
          {
auto checked354 = frameBase(m_state);
if (!checked354) return std::unexpected(std::move(checked354.error()));
image = strf("{}:{}.{}{}", m_backArmFrameset, std::move(*checked354), armStateSeq, prefix);
}
        auto checked355 = m_root.imageMetadataDatabase();
if (!checked355) return std::unexpected(std::move(checked355.error()));
auto checked356 = Drawable::makeImage(*std::move(*checked355), std::move(image), 1.0f / TilePixels, true, position);
if (!checked356) return std::unexpected(std::move(checked356.error()));
auto drawable = std::move(*checked356);
        auto checked357 = m_root.imageMetadataDatabase();
if (!checked357) return std::unexpected(std::move(checked357.error()));
auto checked358 = drawable.imagePart().addDirectives(*std::move(*checked357), bodyDirectives, true);
if (!checked358) return std::unexpected(std::move(checked358.error()));
        if (dance.has_value())
          drawable.rotate(danceStep->backArmRotation);
        addDrawable(std::move(drawable), m_bodyFullbright);
      }
      for (std::uint8_t i : fashion.wornChestsLegs) {
        if (i == 0)
          break;
        auto const& wearable = fashion.wearables[std::size_t(i) - 1];
        if (std::holds_alternative<WornChest>(wearable) && !std::get<WornChest>(wearable).backSleeveFrameset.empty()) {
          auto const& chest = std::get<WornChest>(wearable);
          if (holdingItem) {
            auto checked359 = backArmDrawable(chest.backSleeveFrameset, chest.directives);
if (!checked359) return std::unexpected(std::move(checked359.error()));
addDrawable(std::move(*checked359), chest.fullbright);
          } else {
            String image;
            Vec2F position;
            auto prefix = chest.directives.prefix();
            if (dance.has_value() && danceStep->backArmFrame) {
              image = strf("{}:{}{}", chest.backSleeveFrameset, *danceStep->backArmFrame, prefix);
              position = danceStep->backArmOffset / TilePixels;
            } else if (m_state == Idle) {
              image = strf("{}:{}{}", chest.backSleeveFrameset, m_identity.personality.armIdle, prefix);
              position = m_identity.personality.armOffset / TilePixels;
            } else
              {
auto checked360 = frameBase(m_state);
if (!checked360) return std::unexpected(std::move(checked360.error()));
image = strf("{}:{}.{}{}", chest.backSleeveFrameset, std::move(*checked360), armStateSeq, prefix);
}
            auto checked361 = m_root.imageMetadataDatabase();
if (!checked361) return std::unexpected(std::move(checked361.error()));
auto checked362 = Drawable::makeImage(*std::move(*checked361), std::move(image), 1.0f / TilePixels, true, position);
if (!checked362) return std::unexpected(std::move(checked362.error()));
auto drawable = std::move(*checked362);
            auto checked363 = m_root.imageMetadataDatabase();
if (!checked363) return std::unexpected(std::move(checked363.error()));
auto checked364 = drawable.imagePart().addDirectives(*std::move(*checked363), chest.directives, true);
if (!checked364) return std::unexpected(std::move(checked364.error()));
            if (dance.has_value())
              drawable.rotate(danceStep->backArmRotation);
            addDrawable(std::move(drawable), chest.fullbright);
          }
        }
      }
      return {};
};

    if (backHand.holdingItem && !dance.has_value() && withItems) {
      auto drawItem = [&]() {
        for (auto backHandItem : backHand.itemDrawables) {
          backHandItem.translate(m_frontHandPosition + backArmFrameOffset + m_backArmOffset);
          backHandItem.rotate(backItemAngle, backArmFrameOffset + m_backArmRotationCenter + m_backArmOffset);
          addDrawable(std::move(backHandItem));
        }
      };
      if (!m_twoHanded && backHand.outsideOfHand)
        drawItem();

      auto checked365 = drawBackArmAndSleeves(true);
if (!checked365) return std::unexpected(std::move(checked365.error()));

      if (!m_twoHanded && !backHand.outsideOfHand)
        drawItem();
    } else {
      auto checked366 = drawBackArmAndSleeves(false);
if (!checked366) return std::unexpected(std::move(checked366.error()));
    }

    auto addHeadDrawable = [&](Drawable drawable, bool forceFullbright = false) {
      if (m_facingDirection == Direction::Left)
        drawable.scale(Vec2F(-1, 1));
      drawable.fullbright |= forceFullbright;
      applyHeadRotation(drawable);
      drawables.append(std::move(drawable));
    };

    if (!m_headFrameset.empty() && !m_bodyHidden) {
      String image = strf("{}:normal", m_headFrameset);
      auto checked367 = m_root.imageMetadataDatabase();
if (!checked367) return std::unexpected(std::move(checked367.error()));
auto checked368 = Drawable::makeImage(*std::move(*checked367), std::move(image), 1.0f / TilePixels, true, headPosition);
if (!checked368) return std::unexpected(std::move(checked368.error()));
auto drawable = std::move(*checked368);
      auto checked369 = m_root.imageMetadataDatabase();
if (!checked369) return std::unexpected(std::move(checked369.error()));
auto checked370 = drawable.imagePart().addDirectives(*std::move(*checked369), getBodyDirectives(), true);
if (!checked370) return std::unexpected(std::move(checked370.error()));
      addHeadDrawable(std::move(drawable), m_bodyFullbright);
    }

    if (!m_emoteFrameset.empty() && !m_bodyHidden) {
      auto emoteDirectives = getEmoteDirectives();
      auto checked371 = emoteFrameBase(m_emoteState);
if (!checked371) return std::unexpected(std::move(checked371.error()));
String image = strf("{}:{}.{}{}", m_emoteFrameset, std::move(*checked371), emoteStateSeq, emoteDirectives.prefix());
      auto checked372 = m_root.imageMetadataDatabase();
if (!checked372) return std::unexpected(std::move(checked372.error()));
auto checked373 = Drawable::makeImage(*std::move(*checked372), std::move(image), 1.0f / TilePixels, true, headPosition);
if (!checked373) return std::unexpected(std::move(checked373.error()));
auto drawable = std::move(*checked373);
      auto checked374 = m_root.imageMetadataDatabase();
if (!checked374) return std::unexpected(std::move(checked374.error()));
auto checked375 = drawable.imagePart().addDirectives(*std::move(*checked374), emoteDirectives, true);
if (!checked375) return std::unexpected(std::move(checked375.error()));
      addHeadDrawable(std::move(drawable), m_bodyFullbright);
    }

    if (!m_hairFrameset.empty() && !m_bodyHidden) {
      String image = strf("{}:normal", m_hairFrameset);
      auto checked376 = m_root.imageMetadataDatabase();
if (!checked376) return std::unexpected(std::move(checked376.error()));
auto checked377 = Drawable::makeImage(*std::move(*checked376), std::move(image), 1.0f / TilePixels, true, headPosition);
if (!checked377) return std::unexpected(std::move(checked377.error()));
auto drawable = std::move(*checked377);
      auto checked378 = m_root.imageMetadataDatabase();
if (!checked378) return std::unexpected(std::move(checked378.error()));
auto checked379 = drawable.imagePart().addDirectives(*std::move(*checked378), getHairDirectives(), true);
if (!checked379) return std::unexpected(std::move(checked379.error()));
auto checked380 = m_root.imageMetadataDatabase();
if (!checked380) return std::unexpected(std::move(checked380.error()));
auto checked381 = checked379->get().addDirectivesGroup(*std::move(*checked380), fashion.helmetMaskDirectivesGroup, true);
if (!checked381) return std::unexpected(std::move(checked381.error()));
      addHeadDrawable(std::move(drawable), m_bodyFullbright);
    }

    if (!m_bodyFrameset.empty() && !m_bodyHidden) {
      auto bodyDirectives = getBodyDirectives();
      auto prefix = bodyDirectives.prefix();
      String frameName;
      if (dance.has_value() && danceStep->bodyFrame)
        frameName = strf("{}{}", *danceStep->bodyFrame, prefix);
      else if (m_state == Idle)
        frameName = strf("{}{}", m_identity.personality.idle, prefix);
      else
        {
auto checked382 = frameBase(m_state);
if (!checked382) return std::unexpected(std::move(checked382.error()));
frameName = strf("{}.{}{}", std::move(*checked382), bodyStateSeq, prefix);
}
      String image = strf("{}:{}",m_bodyFrameset,frameName);
      auto checked383 = m_root.imageMetadataDatabase();
if (!checked383) return std::unexpected(std::move(checked383.error()));
auto checked384 = Drawable::makeImage(*std::move(*checked383), m_useBodyHeadMask ? image : std::move(image), 1.0f / TilePixels, true, {});
if (!checked384) return std::unexpected(std::move(checked384.error()));
auto drawable = std::move(*checked384);
      auto checked385 = m_root.imageMetadataDatabase();
if (!checked385) return std::unexpected(std::move(checked385.error()));
auto checked386 = drawable.imagePart().addDirectives(*std::move(*checked385), bodyDirectives, true);
if (!checked386) return std::unexpected(std::move(checked386.error()));
      if (m_useBodyMask && !m_bodyMaskFrameset.empty()) {
        String maskImage = strf("{}:{}",m_bodyMaskFrameset,frameName);
        Directives maskDirectives = "?addmask="+maskImage+";0;0";
        auto checked387 = m_root.imageMetadataDatabase();
if (!checked387) return std::unexpected(std::move(checked387.error()));
auto checked388 = drawable.imagePart().addDirectives(*std::move(*checked387), maskDirectives, true);
if (!checked388) return std::unexpected(std::move(checked388.error()));
      }
      addDrawable(std::move(drawable), m_bodyFullbright);
      if (m_useBodyHeadMask && !m_bodyHeadMaskFrameset.empty()) {
        String maskImage = strf("{}:{}",m_bodyHeadMaskFrameset,frameName);
        Directives maskDirectives = "?addmask="+maskImage+";0;0";
        auto checked389 = m_root.imageMetadataDatabase();
if (!checked389) return std::unexpected(std::move(checked389.error()));
auto checked390 = Drawable::makeImage(*std::move(*checked389), std::move(image), 1.0f / TilePixels, true, {});
if (!checked390) return std::unexpected(std::move(checked390.error()));
auto drawable = std::move(*checked390);
        auto checked391 = m_root.imageMetadataDatabase();
if (!checked391) return std::unexpected(std::move(checked391.error()));
auto checked392 = drawable.imagePart().addDirectives(*std::move(*checked391), bodyDirectives, true);
if (!checked392) return std::unexpected(std::move(checked392.error()));
        auto checked393 = m_root.imageMetadataDatabase();
if (!checked393) return std::unexpected(std::move(checked393.error()));
auto checked394 = drawable.imagePart().addDirectives(*std::move(*checked393), maskDirectives, true);
if (!checked394) return std::unexpected(std::move(checked394.error()));
        addHeadDrawable(std::move(drawable), m_bodyFullbright);
      }
    }

    for (std::uint8_t i : fashion.wornChestsLegs) {
      if (i == 0)
        break;
      Wearable& wearable = fashion.wearables[std::size_t(i) - 1];
      if (std::holds_alternative<WornLegs>(wearable) && !std::get<WornLegs>(wearable).frameset.empty()) {
        auto const& legs = std::get<WornLegs>(wearable);
        String image;
        auto prefix = legs.directives.prefix();
        if (dance.has_value() && danceStep->bodyFrame)
          image = strf("{}:{}{}", legs.frameset, *danceStep->bodyFrame, prefix);
        else if (m_state == Idle)
          image = strf("{}:{}{}", legs.frameset, m_identity.personality.idle, prefix);
        else
          {
auto checked395 = frameBase(m_state);
if (!checked395) return std::unexpected(std::move(checked395.error()));
image = strf("{}:{}.{}{}", legs.frameset, std::move(*checked395), bodyStateSeq, prefix);
}
        auto checked396 = m_root.imageMetadataDatabase();
if (!checked396) return std::unexpected(std::move(checked396.error()));
auto checked397 = Drawable::makeImage(*std::move(*checked396), std::move(image), 1.0f / TilePixels, true, {});
if (!checked397) return std::unexpected(std::move(checked397.error()));
auto drawable = std::move(*checked397);
        auto checked398 = m_root.imageMetadataDatabase();
if (!checked398) return std::unexpected(std::move(checked398.error()));
auto checked399 = drawable.imagePart().addDirectives(*std::move(*checked398), legs.directives, true);
if (!checked399) return std::unexpected(std::move(checked399.error()));
        addDrawable(std::move(drawable), legs.fullbright);
      } else {
        if (std::holds_alternative<WornChest>(wearable) && !std::get<WornChest>(wearable).frameset.empty()) {
          auto const& chest = std::get<WornChest>(wearable);
          String image;
          Vec2F position;
          auto prefix = chest.directives.prefix();
          if (dance.has_value() && danceStep->bodyFrame)
            image = strf("{}:{}{}", chest.frameset, *danceStep->bodyFrame, prefix);
          else if (m_state == Run)
            image = strf("{}:run{}", chest.frameset, prefix);
          else if (m_state == Idle)
            image = strf("{}:{}{}", chest.frameset, m_identity.personality.idle, prefix);
          else if (m_state == Duck)
            image = strf("{}:duck{}", chest.frameset, prefix);
          else if ((m_state == Swim) || (m_state == SwimIdle))
            image = strf("{}:swim{}", chest.frameset, prefix);
          else
            image = strf("{}:chest.1{}", chest.frameset, prefix);
          if (m_state != Duck)
            position[1] += bobYOffset;
          auto checked400 = m_root.imageMetadataDatabase();
if (!checked400) return std::unexpected(std::move(checked400.error()));
auto checked401 = Drawable::makeImage(*std::move(*checked400), std::move(image), 1.0f / TilePixels, true, position);
if (!checked401) return std::unexpected(std::move(checked401.error()));
auto drawable = std::move(*checked401);
          auto checked402 = m_root.imageMetadataDatabase();
if (!checked402) return std::unexpected(std::move(checked402.error()));
auto checked403 = drawable.imagePart().addDirectives(*std::move(*checked402), chest.directives, true);
if (!checked403) return std::unexpected(std::move(checked403.error()));
          addDrawable(std::move(drawable), chest.fullbright);
        }
      }
    }

    if (!m_facialHairFrameset.empty() && !m_bodyHidden) {
      String image = strf("{}:normal", m_facialHairFrameset);
      auto checked404 = m_root.imageMetadataDatabase();
if (!checked404) return std::unexpected(std::move(checked404.error()));
auto checked405 = Drawable::makeImage(*std::move(*checked404), std::move(image), 1.0f / TilePixels, true, headPosition);
if (!checked405) return std::unexpected(std::move(checked405.error()));
auto drawable = std::move(*checked405);
      auto checked406 = m_root.imageMetadataDatabase();
if (!checked406) return std::unexpected(std::move(checked406.error()));
auto checked407 = drawable.imagePart().addDirectives(*std::move(*checked406), getFacialHairDirectives(), true);
if (!checked407) return std::unexpected(std::move(checked407.error()));
auto checked408 = m_root.imageMetadataDatabase();
if (!checked408) return std::unexpected(std::move(checked408.error()));
auto checked409 = checked407->get().addDirectivesGroup(*std::move(*checked408), fashion.helmetMaskDirectivesGroup, true);
if (!checked409) return std::unexpected(std::move(checked409.error()));
      addHeadDrawable(std::move(drawable), m_bodyFullbright);
    }

    if (!m_facialMaskFrameset.empty() && !m_bodyHidden) {
      String image = strf("{}:normal", m_facialMaskFrameset);
      auto checked410 = m_root.imageMetadataDatabase();
if (!checked410) return std::unexpected(std::move(checked410.error()));
auto checked411 = Drawable::makeImage(*std::move(*checked410), std::move(image), 1.0f / TilePixels, true, headPosition);
if (!checked411) return std::unexpected(std::move(checked411.error()));
auto drawable = std::move(*checked411);
      auto checked412 = m_root.imageMetadataDatabase();
if (!checked412) return std::unexpected(std::move(checked412.error()));
auto checked413 = drawable.imagePart().addDirectives(*std::move(*checked412), getFacialMaskDirectives(), true);
if (!checked413) return std::unexpected(std::move(checked413.error()));
auto checked414 = m_root.imageMetadataDatabase();
if (!checked414) return std::unexpected(std::move(checked414.error()));
auto checked415 = checked413->get().addDirectivesGroup(*std::move(*checked414), fashion.helmetMaskDirectivesGroup, true);
if (!checked415) return std::unexpected(std::move(checked415.error()));
      addHeadDrawable(std::move(drawable));
    }

    for (std::uint8_t i : fashion.wornHeads) {
      if (i == 0)
        break;
      auto& head = std::get<WornHead>(fashion.wearables[std::size_t(i) - 1]);
      if (!head.frameset.empty()) {
        String image = strf("{}:normal{}", head.frameset, head.directives.prefix());
        auto checked416 = m_root.imageMetadataDatabase();
if (!checked416) return std::unexpected(std::move(checked416.error()));
auto checked417 = Drawable::makeImage(*std::move(*checked416), std::move(image), 1.0f / TilePixels, true, headPosition);
if (!checked417) return std::unexpected(std::move(checked417.error()));
auto drawable = std::move(*checked417);
        auto checked418 = m_root.imageMetadataDatabase();
if (!checked418) return std::unexpected(std::move(checked418.error()));
auto checked419 = drawable.imagePart().addDirectives(*std::move(*checked418), head.directives, true);
if (!checked419) return std::unexpected(std::move(checked419.error()));
        addHeadDrawable(std::move(drawable), head.fullbright);
      }
    }

    auto frontArmDrawable = [&](String const& frameSet, Directives const& directives) -> std::expected<Drawable, StarException> {
      String image = strf("{}:{}{}", frameSet, frontHand.frontFrame, directives.prefix());
      auto checked420 = m_root.imageMetadataDatabase();
if (!checked420) return std::unexpected(std::move(checked420.error()));
auto checked421 = Drawable::makeImage(*std::move(*checked420), image, 1.0f / TilePixels, true, frontArmFrameOffset);
if (!checked421) return std::unexpected(std::move(checked421.error()));
Drawable frontArm = std::move(*checked421);
      auto checked422 = m_root.imageMetadataDatabase();
if (!checked422) return std::unexpected(std::move(checked422.error()));
auto checked423 = frontArm.imagePart().addDirectives(*std::move(*checked422), directives, true);
if (!checked423) return std::unexpected(std::move(checked423.error()));
      auto checked424 = m_root.imageMetadataDatabase();
if (!checked424) return std::unexpected(std::move(checked424.error()));
auto checked425 = frontArm.imagePart().addDirectives(*std::move(*checked424), frontHand.frontDirectives, true);
if (!checked425) return std::unexpected(std::move(checked425.error()));
      frontArm.rotate(frontAngle, frontArmFrameOffset + m_frontArmRotationCenter);
      return frontArm;
    };


    auto drawFrontArmAndSleeves = [&](bool holdingItem) -> std::expected<void, StarException> {
      auto& bodyDirectives = getBodyDirectives();
      if (holdingItem && !m_bodyHidden)
        {
auto checked426 = frontArmDrawable(m_frontArmFrameset, bodyDirectives);
if (!checked426) return std::unexpected(std::move(checked426.error()));
addDrawable(std::move(*checked426), m_bodyFullbright);
}
      else if (!m_frontArmFrameset.empty() && !m_bodyHidden) {
        String image;
        Vec2F position;
        auto prefix = bodyDirectives.prefix();
        if (dance.has_value() && danceStep->frontArmFrame) {
          image = strf("{}:{}{}", m_frontArmFrameset, *danceStep->frontArmFrame, prefix);
          position = danceStep->frontArmOffset / TilePixels;
        } else if (m_state == Idle) {
          image = strf("{}:{}{}", m_frontArmFrameset, m_identity.personality.armIdle, prefix);
          position = m_identity.personality.armOffset / TilePixels;
        } else
          {
auto checked427 = frameBase(m_state);
if (!checked427) return std::unexpected(std::move(checked427.error()));
image = strf("{}:{}.{}{}", m_frontArmFrameset, std::move(*checked427), armStateSeq, prefix);
}
        auto checked428 = m_root.imageMetadataDatabase();
if (!checked428) return std::unexpected(std::move(checked428.error()));
auto checked429 = Drawable::makeImage(*std::move(*checked428), std::move(image), 1.0f / TilePixels, true, position);
if (!checked429) return std::unexpected(std::move(checked429.error()));
auto drawable = std::move(*checked429);
        auto checked430 = m_root.imageMetadataDatabase();
if (!checked430) return std::unexpected(std::move(checked430.error()));
auto checked431 = drawable.imagePart().addDirectives(*std::move(*checked430), bodyDirectives, true);
if (!checked431) return std::unexpected(std::move(checked431.error()));
        if (dance.has_value())
          drawable.rotate(danceStep->frontArmRotation);
        addDrawable(drawable, m_bodyFullbright);
      }
      for (std::uint8_t i : fashion.wornChestsLegs) {
        if (i == 0)
          break;
        auto const& wearable = fashion.wearables[std::size_t(i) - 1];
        if (std::holds_alternative<WornChest>(wearable) && !std::get<WornChest>(wearable).frontSleeveFrameset.empty()) {
          auto const& chest = std::get<WornChest>(wearable);
          if (holdingItem) {
            auto checked432 = frontArmDrawable(chest.frontSleeveFrameset, chest.directives);
if (!checked432) return std::unexpected(std::move(checked432.error()));
addDrawable(std::move(*checked432), chest.fullbright);
          } else {
            String image;
            Vec2F position;
            auto prefix = chest.directives.prefix();
            if (dance.has_value() && danceStep->frontArmFrame) {
              image = strf("{}:{}{}", chest.frontSleeveFrameset, *danceStep->frontArmFrame, prefix);
              position = danceStep->frontArmOffset / TilePixels;
            } else if (m_state == Idle) {
              image = strf("{}:{}{}", chest.frontSleeveFrameset, m_identity.personality.armIdle, prefix);
              position = m_identity.personality.armOffset / TilePixels;
            } else
              {
auto checked433 = frameBase(m_state);
if (!checked433) return std::unexpected(std::move(checked433.error()));
image = strf("{}:{}.{}{}", chest.frontSleeveFrameset, std::move(*checked433), armStateSeq, prefix);
}
            auto checked434 = m_root.imageMetadataDatabase();
if (!checked434) return std::unexpected(std::move(checked434.error()));
auto checked435 = Drawable::makeImage(*std::move(*checked434), image, 1.0f / TilePixels, true, position);
if (!checked435) return std::unexpected(std::move(checked435.error()));
auto drawable = std::move(*checked435);
            auto checked436 = m_root.imageMetadataDatabase();
if (!checked436) return std::unexpected(std::move(checked436.error()));
auto checked437 = drawable.imagePart().addDirectives(*std::move(*checked436), chest.directives, true);
if (!checked437) return std::unexpected(std::move(checked437.error()));
            if (dance.has_value())
              drawable.rotate(danceStep->frontArmRotation);
            addDrawable(drawable, chest.fullbright);
          }
        }
      }
      return {};
};

    if (frontHand.holdingItem && !dance.has_value() && withItems) {
      auto drawItem = [&]() {
        for (auto frontHandItem : frontHand.itemDrawables) {
          frontHandItem.translate(m_frontHandPosition + frontArmFrameOffset);
          frontHandItem.rotate(frontItemAngle, frontArmFrameOffset + m_frontArmRotationCenter);
          addDrawable(std::move(frontHandItem));
        }
      };
      if (!frontHand.outsideOfHand)
        drawItem();

      auto checked438 = drawFrontArmAndSleeves(true);
if (!checked438) return std::unexpected(std::move(checked438.error()));

      if (frontHand.outsideOfHand)
        drawItem();
    } else {
      auto checked439 = drawFrontArmAndSleeves(false);
if (!checked439) return std::unexpected(std::move(checked439.error()));
    }

    if (m_drawVaporTrail) {
      auto image = strf("{}:{}",
          m_vaporTrailFrameset,
          m_timing.genericSeq(lerp(interpolation, m_previousAnimationTimer, m_animationTimer), m_vaporTrailCycle, m_vaporTrailFrames, true));
      auto checked440 = m_root.imageMetadataDatabase();
if (!checked440) return std::unexpected(std::move(checked440.error()));
auto checked441 = Drawable::makeImage(*std::move(*checked440), AssetPath::split(image), 1.0f / TilePixels, true, {});
if (!checked441) return std::unexpected(std::move(checked441.error()));
addDrawable(std::move(*checked441));
    }

    if (withItems) {
      if (m_primaryHand.nonRotatedDrawables.size())
        drawables.insertAllAt(0, m_primaryHand.nonRotatedDrawables);
      if (m_altHand.nonRotatedDrawables.size())
        drawables.insertAllAt(0, m_altHand.nonRotatedDrawables);
    }

    for (auto& drawable : drawables) {
      drawable.translate(m_globalOffset);
      if (withRotationAndScale) {
        if (scale.x() != 1.f || scale.y() != 1.f)
          drawable.scale(scale);
        if (rotation != 0.f)
          drawable.rotate(rotation);
      }
      drawable.rebase();
    }
  }

  return drawables;
}

std::expected<List<Drawable>, StarException> Humanoid::renderPortrait(PortraitMode mode) const {
  auto& fashion = *m_fashion;
  ((Humanoid*)this)->refreshWearables(fashion); // bleh
  List<Drawable> drawables;

  if (m_useAnimation) {
    auto checked442 = NetworkedAnimator::create(*m_networkedAnimator);
if (!checked442) return std::unexpected(std::move(checked442.error()));
auto portraitAnimator = std::move(*checked442);
    portraitAnimator->setFlipped(false);
    portraitAnimator->setPartDrawables(m_frontItemPart, {});
    portraitAnimator->setPartDrawables(m_backItemPart, {});
    auto checked443 = portraitAnimator->resetLocalTransformationGroup("headRotation");
if (!checked443) return std::unexpected(std::move(checked443.error()));
    for (std::uint8_t i : fashion.wornBacks) {
      if (i == 0)
        break;
      auto checked444 = portraitAnimator->resetLocalTransformationGroup("backCosmetic" + toString(i) + "Rotation");
if (!checked444) return std::unexpected(std::move(checked444.error()));
    }
    auto checked445 = portraitAnimator->resetLocalTransformationGroup("frontArmRotation");
if (!checked445) return std::unexpected(std::move(checked445.error()));
    auto checked446 = portraitAnimator->resetLocalTransformationGroup("backArmRotation");
if (!checked446) return std::unexpected(std::move(checked446.error()));
    auto checked447 = portraitAnimator->setLocalState("frontArm", "idle");
if (!checked447) return std::unexpected(std::move(checked447.error()));
    auto checked448 = portraitAnimator->setLocalState("backArm", "idle");
if (!checked448) return std::unexpected(std::move(checked448.error()));

    if (auto animationStates = m_portraitAnimationStates.maybe(mode))
      for (auto args : *animationStates)
        {
auto checked449 = portraitAnimator->setLocalState(args.first, args.second.state, args.second.startNew, args.second.reverse);
if (!checked449) return std::unexpected(std::move(checked449.error()));
}

    if (mode == PortraitMode::FullNeutral || mode == PortraitMode::FullNeutralNude) {
      auto checked450 = m_root.speciesDatabase();
if (!checked450) return std::unexpected(std::move(checked450.error()));
auto checked451 = std::move(*checked450)->species(m_identity.species);
if (!checked451) return std::unexpected(std::move(checked451.error()));
auto checked452 = std::move(*checked451)->personalities().at(0);
if (!checked452) return std::unexpected(std::move(checked452.error()));
auto const& personality = checked452->get();
      portraitAnimator->setLocalTag("personalityIdle", personality.idle);
      portraitAnimator->setLocalTag("personalityArmIdle", personality.armIdle);
      auto checked453 = portraitAnimator->resetLocalTransformationGroup("personalityHeadOffset");
if (!checked453) return std::unexpected(std::move(checked453.error()));
      auto checked454 = portraitAnimator->translateLocalTransformationGroup("personalityHeadOffset", personality.headOffset / TilePixels);
if (!checked454) return std::unexpected(std::move(checked454.error()));
      auto checked455 = portraitAnimator->resetLocalTransformationGroup("personalityArmOffset");
if (!checked455) return std::unexpected(std::move(checked455.error()));
      auto checked456 = portraitAnimator->translateLocalTransformationGroup("personalityArmOffset", personality.armOffset / TilePixels);
if (!checked456) return std::unexpected(std::move(checked456.error()));
    }
    if (mode == PortraitMode::FullNude || mode == PortraitMode::FullNeutralNude) {
      portraitAnimator->setLocalTag("helmetMaskDirectives");
      auto setTags = [&](HashMap<String,String> tags) {
      if (m_useAnimation)
        for (auto tag : tags) {
          portraitAnimator->setLocalTag(tag.first);
        }
      };
      for (std::uint8_t i = 0; i != fashion.wearables.size(); ++i) {
        auto& wearable = fashion.wearables[i];
        if (std::holds_alternative<std::monostate>(wearable))
          continue;
        if (std::holds_alternative<WornHead>(wearable)) {
          setTags(std::get<WornHead>(wearable).animationTags);
        } else if (std::holds_alternative<WornChest>(wearable)) {
          setTags(std::get<WornChest>(wearable).animationTags);
        } else if (std::holds_alternative<WornLegs>(wearable)) {
          setTags(std::get<WornLegs>(wearable).animationTags);
        } else if (std::holds_alternative<WornBack>(wearable)) {
          setTags(std::get<WornBack>(wearable).animationTags);
        }
      }
    }
    auto checked457 = portraitAnimator->update(0, {});
if (!checked457) return std::unexpected(std::move(checked457.error()));

    auto checked458 = portraitAnimator->drawables();
if (!checked458) return std::unexpected(std::move(checked458.error()));
drawables = std::move(*checked458);
    Drawable::scaleAll(drawables, TilePixels);
  } else {
    int emoteStateSeq = m_timing.emoteStateSeq(m_emoteAnimationTimer, m_emoteState);

    auto addDrawable = [&](Drawable&& drawable) -> std::expected<void, StarException> {
      if (mode != PortraitMode::Full && mode != PortraitMode::FullNeutral
        && mode != PortraitMode::FullNude && mode != PortraitMode::FullNeutralNude) {
        // TODO: make this configurable
        auto checked459 = m_root.imageMetadataDatabase();
if (!checked459) return std::unexpected(std::move(checked459.error()));
auto checked460 = drawable.imagePart().addDirectives(*std::move(*checked459), String("addmask=/humanoid/portraitMask.png;0;0"), false);
if (!checked460) return std::unexpected(std::move(checked460.error()));
      }
      drawables.append(std::move(drawable));
      return {};
    };

    bool dressed = !(mode == PortraitMode::FullNude || mode == PortraitMode::FullNeutralNude);

    auto personality = m_identity.personality;
    if (mode == PortraitMode::FullNeutral || mode == PortraitMode::FullNeutralNude)
      {
auto checked461 = m_root.speciesDatabase();
if (!checked461) return std::unexpected(std::move(checked461.error()));
auto checked462 = std::move(*checked461)->species(m_identity.species);
if (!checked462) return std::unexpected(std::move(checked462.error()));
auto checked463 = std::move(*checked462)->personalities().at(0);
if (!checked463) return std::unexpected(std::move(checked463.error()));
personality = checked463->get();
}

    if (mode != PortraitMode::Head) {
      if (!m_backArmFrameset.empty()) {
        auto& bodyDirectives = getBodyDirectives();
        String image = strf("{}:{}{}", m_backArmFrameset, personality.armIdle, bodyDirectives.prefix());
        auto checked464 = m_root.imageMetadataDatabase();
if (!checked464) return std::unexpected(std::move(checked464.error()));
auto checked465 = Drawable::makeImage(*std::move(*checked464), std::move(image), 1.0f, true, personality.armOffset);
if (!checked465) return std::unexpected(std::move(checked465.error()));
Drawable drawable = std::move(*checked465);
        auto checked466 = m_root.imageMetadataDatabase();
if (!checked466) return std::unexpected(std::move(checked466.error()));
auto checked467 = drawable.imagePart().addDirectives(*std::move(*checked466), bodyDirectives, true);
if (!checked467) return std::unexpected(std::move(checked467.error()));
        auto checked468 = addDrawable(std::move(drawable));
if (!checked468) return std::unexpected(std::move(checked468.error()));
      }
      if (dressed) {
        for (std::uint8_t i : fashion.wornChestsLegs) {
          if (i == 0)
            break;
          auto const& wearable = fashion.wearables[std::size_t(i) - 1];
          if (std::holds_alternative<WornChest>(wearable) && !std::get<WornChest>(wearable).backSleeveFrameset.empty()) {
            auto const& chest = std::get<WornChest>(wearable);
            String image = strf("{}:{}{}", chest.backSleeveFrameset, personality.armIdle, chest.directives.prefix());
            auto checked469 = m_root.imageMetadataDatabase();
if (!checked469) return std::unexpected(std::move(checked469.error()));
auto checked470 = Drawable::makeImage(*std::move(*checked469), std::move(image), 1.0f, true, personality.armOffset);
if (!checked470) return std::unexpected(std::move(checked470.error()));
Drawable drawable = std::move(*checked470);
            auto checked471 = m_root.imageMetadataDatabase();
if (!checked471) return std::unexpected(std::move(checked471.error()));
auto checked472 = drawable.imagePart().addDirectives(*std::move(*checked471), chest.directives, true);
if (!checked472) return std::unexpected(std::move(checked472.error()));
            auto checked473 = addDrawable(std::move(drawable));
if (!checked473) return std::unexpected(std::move(checked473.error()));
          }
        }
      }
      if (mode != PortraitMode::Bust && dressed) {
        for (std::uint8_t i : fashion.wornBacks) {
          if (i == 0)
            break;
          auto& back = std::get<WornBack>(fashion.wearables[std::size_t(i) - 1]);
          if (!back.frameset.empty()) {
            auto backDirectives = back.directives;
            String image = strf("{}:{}{}", back.frameset, personality.idle, backDirectives.prefix());
            auto checked474 = m_root.imageMetadataDatabase();
if (!checked474) return std::unexpected(std::move(checked474.error()));
auto checked475 = Drawable::makeImage(*std::move(*checked474), std::move(image), 1.0f, true, {});
if (!checked475) return std::unexpected(std::move(checked475.error()));
Drawable drawable = std::move(*checked475);
            auto checked476 = m_root.imageMetadataDatabase();
if (!checked476) return std::unexpected(std::move(checked476.error()));
auto checked477 = drawable.imagePart().addDirectives(*std::move(*checked476), backDirectives, true);
if (!checked477) return std::unexpected(std::move(checked477.error()));
            auto checked478 = addDrawable(std::move(drawable));
if (!checked478) return std::unexpected(std::move(checked478.error()));
          }
        }
      }
    }

    if (!m_headFrameset.empty()) {
      auto& bodyDirectives = getBodyDirectives();
      String image = strf("{}:normal{}", m_headFrameset, bodyDirectives.prefix());
      auto checked479 = m_root.imageMetadataDatabase();
if (!checked479) return std::unexpected(std::move(checked479.error()));
auto checked480 = Drawable::makeImage(*std::move(*checked479), std::move(image), 1.0f, true, personality.headOffset);
if (!checked480) return std::unexpected(std::move(checked480.error()));
Drawable drawable = std::move(*checked480);
      auto checked481 = m_root.imageMetadataDatabase();
if (!checked481) return std::unexpected(std::move(checked481.error()));
auto checked482 = drawable.imagePart().addDirectives(*std::move(*checked481), bodyDirectives, true);
if (!checked482) return std::unexpected(std::move(checked482.error()));
      auto checked483 = addDrawable(std::move(drawable));
if (!checked483) return std::unexpected(std::move(checked483.error()));
    }

    if (!m_emoteFrameset.empty()) {
      auto& emoteDirectives = getEmoteDirectives();
      auto checked484 = emoteFrameBase(m_emoteState);
if (!checked484) return std::unexpected(std::move(checked484.error()));
String image = strf("{}:{}.{}{}", m_emoteFrameset, std::move(*checked484), emoteStateSeq, emoteDirectives.prefix());
      auto checked485 = m_root.imageMetadataDatabase();
if (!checked485) return std::unexpected(std::move(checked485.error()));
auto checked486 = Drawable::makeImage(*std::move(*checked485), std::move(image), 1.0f, true, personality.headOffset);
if (!checked486) return std::unexpected(std::move(checked486.error()));
Drawable drawable = std::move(*checked486);
      auto checked487 = m_root.imageMetadataDatabase();
if (!checked487) return std::unexpected(std::move(checked487.error()));
auto checked488 = drawable.imagePart().addDirectives(*std::move(*checked487), emoteDirectives, true);
if (!checked488) return std::unexpected(std::move(checked488.error()));
      auto checked489 = addDrawable(std::move(drawable));
if (!checked489) return std::unexpected(std::move(checked489.error()));
    }

    if (!m_hairFrameset.empty()) {
      auto& hairDirectives = getHairDirectives();
      String image = strf("{}:normal{}", m_hairFrameset, hairDirectives.prefix());
      auto checked490 = m_root.imageMetadataDatabase();
if (!checked490) return std::unexpected(std::move(checked490.error()));
auto checked491 = Drawable::makeImage(*std::move(*checked490), std::move(image), 1.0f, true, personality.headOffset);
if (!checked491) return std::unexpected(std::move(checked491.error()));
Drawable drawable = std::move(*checked491);
      auto checked492 = m_root.imageMetadataDatabase();
if (!checked492) return std::unexpected(std::move(checked492.error()));
auto checked493 = drawable.imagePart().addDirectives(*std::move(*checked492), hairDirectives, true);
if (!checked493) return std::unexpected(std::move(checked493.error()));
auto checked494 = m_root.imageMetadataDatabase();
if (!checked494) return std::unexpected(std::move(checked494.error()));
auto checked495 = checked493->get().addDirectivesGroup(*std::move(*checked494), fashion.helmetMaskDirectivesGroup, true);
if (!checked495) return std::unexpected(std::move(checked495.error()));
      auto checked496 = addDrawable(std::move(drawable));
if (!checked496) return std::unexpected(std::move(checked496.error()));
    }

    if (!m_bodyFrameset.empty()) {
      auto& bodyDirectives = getBodyDirectives();
      String image = strf("{}:{}{}", m_bodyFrameset, personality.idle, bodyDirectives.prefix());
      auto checked497 = m_root.imageMetadataDatabase();
if (!checked497) return std::unexpected(std::move(checked497.error()));
auto checked498 = Drawable::makeImage(*std::move(*checked497), std::move(image), 1.0f, true, {});
if (!checked498) return std::unexpected(std::move(checked498.error()));
Drawable drawable = std::move(*checked498);
      auto checked499 = m_root.imageMetadataDatabase();
if (!checked499) return std::unexpected(std::move(checked499.error()));
auto checked500 = drawable.imagePart().addDirectives(*std::move(*checked499), bodyDirectives, true);
if (!checked500) return std::unexpected(std::move(checked500.error()));
      auto checked501 = addDrawable(std::move(drawable));
if (!checked501) return std::unexpected(std::move(checked501.error()));
    }

    if (mode != PortraitMode::Head && dressed) {
      for (std::uint8_t i : fashion.wornChestsLegs) {
        if (i == 0)
          break;
        auto& wearable = fashion.wearables[std::size_t(i) - 1];
        if (std::holds_alternative<WornLegs>(wearable) && !std::get<WornLegs>(wearable).frameset.empty()) {
          auto const& legs = std::get<WornLegs>(wearable);
          String image = strf("{}:{}{}", legs.frameset, personality.idle, legs.directives.prefix());
          auto checked502 = m_root.imageMetadataDatabase();
if (!checked502) return std::unexpected(std::move(checked502.error()));
auto checked503 = Drawable::makeImage(*std::move(*checked502), std::move(image), 1.0f, true, {});
if (!checked503) return std::unexpected(std::move(checked503.error()));
Drawable drawable = std::move(*checked503);
          auto checked504 = m_root.imageMetadataDatabase();
if (!checked504) return std::unexpected(std::move(checked504.error()));
auto checked505 = drawable.imagePart().addDirectives(*std::move(*checked504), legs.directives, true);
if (!checked505) return std::unexpected(std::move(checked505.error()));
          auto checked506 = addDrawable(std::move(drawable));
if (!checked506) return std::unexpected(std::move(checked506.error()));
        } else {
          if (std::holds_alternative<WornChest>(wearable) && !std::get<WornChest>(wearable).frameset.empty()) {
            auto const& chest = std::get<WornChest>(wearable);
            String image = strf("{}:{}{}", chest.frameset, personality.idle, chest.directives.prefix());
            auto checked507 = m_root.imageMetadataDatabase();
if (!checked507) return std::unexpected(std::move(checked507.error()));
auto checked508 = Drawable::makeImage(*std::move(*checked507), std::move(image), 1.0f, true, {});
if (!checked508) return std::unexpected(std::move(checked508.error()));
Drawable drawable = std::move(*checked508);
            auto checked509 = m_root.imageMetadataDatabase();
if (!checked509) return std::unexpected(std::move(checked509.error()));
auto checked510 = drawable.imagePart().addDirectives(*std::move(*checked509), chest.directives, true);
if (!checked510) return std::unexpected(std::move(checked510.error()));
            auto checked511 = addDrawable(std::move(drawable));
if (!checked511) return std::unexpected(std::move(checked511.error()));
          }
        }
      }
    }

    if (!m_facialHairFrameset.empty()) {
      auto facialHairDirectives = getFacialHairDirectives();
      String image = strf("{}:normal{}", m_facialHairFrameset, facialHairDirectives.prefix());
      auto checked512 = m_root.imageMetadataDatabase();
if (!checked512) return std::unexpected(std::move(checked512.error()));
auto checked513 = Drawable::makeImage(*std::move(*checked512), std::move(image), 1.0f, true, personality.headOffset);
if (!checked513) return std::unexpected(std::move(checked513.error()));
Drawable drawable = std::move(*checked513);
      auto checked514 = m_root.imageMetadataDatabase();
if (!checked514) return std::unexpected(std::move(checked514.error()));
auto checked515 = drawable.imagePart().addDirectives(*std::move(*checked514), facialHairDirectives, true);
if (!checked515) return std::unexpected(std::move(checked515.error()));
auto checked516 = m_root.imageMetadataDatabase();
if (!checked516) return std::unexpected(std::move(checked516.error()));
auto checked517 = checked515->get().addDirectivesGroup(*std::move(*checked516), fashion.helmetMaskDirectivesGroup, true);
if (!checked517) return std::unexpected(std::move(checked517.error()));
      auto checked518 = addDrawable(std::move(drawable));
if (!checked518) return std::unexpected(std::move(checked518.error()));
    }

    if (!m_facialMaskFrameset.empty()) {
      auto facialMaskDirectives = getFacialMaskDirectives();
      String image = strf("{}:normal{}", m_facialMaskFrameset, facialMaskDirectives.prefix());
      auto checked519 = m_root.imageMetadataDatabase();
if (!checked519) return std::unexpected(std::move(checked519.error()));
auto checked520 = Drawable::makeImage(*std::move(*checked519), std::move(image), 1.0f, true, personality.headOffset);
if (!checked520) return std::unexpected(std::move(checked520.error()));
Drawable drawable = std::move(*checked520);
      auto checked521 = m_root.imageMetadataDatabase();
if (!checked521) return std::unexpected(std::move(checked521.error()));
auto checked522 = drawable.imagePart().addDirectives(*std::move(*checked521), facialMaskDirectives, true);
if (!checked522) return std::unexpected(std::move(checked522.error()));
auto checked523 = m_root.imageMetadataDatabase();
if (!checked523) return std::unexpected(std::move(checked523.error()));
auto checked524 = checked522->get().addDirectivesGroup(*std::move(*checked523), fashion.helmetMaskDirectivesGroup, true);
if (!checked524) return std::unexpected(std::move(checked524.error()));
      auto checked525 = addDrawable(std::move(drawable));
if (!checked525) return std::unexpected(std::move(checked525.error()));
    }

    if (dressed) {
      for (std::uint8_t i : fashion.wornHeads) {
        if (i == 0)
          break;
        auto& head = std::get<WornHead>(fashion.wearables[std::size_t(i) - 1]);
        if (!head.frameset.empty()) {
          String image = strf("{}:normal{}", head.frameset, head.directives.prefix());
          auto checked526 = m_root.imageMetadataDatabase();
if (!checked526) return std::unexpected(std::move(checked526.error()));
auto checked527 = Drawable::makeImage(*std::move(*checked526), std::move(image), 1.0f, true, personality.headOffset);
if (!checked527) return std::unexpected(std::move(checked527.error()));
Drawable drawable = std::move(*checked527);
          auto checked528 = m_root.imageMetadataDatabase();
if (!checked528) return std::unexpected(std::move(checked528.error()));
auto checked529 = drawable.imagePart().addDirectives(*std::move(*checked528), head.directives, true);
if (!checked529) return std::unexpected(std::move(checked529.error()));
          auto checked530 = addDrawable(std::move(drawable));
if (!checked530) return std::unexpected(std::move(checked530.error()));
        }
      }
    }

    if (mode != PortraitMode::Head) {
      if (!m_frontArmFrameset.empty()) {
        auto bodyDirectives = getBodyDirectives();
        String image = strf("{}:{}{}", m_frontArmFrameset, personality.armIdle, bodyDirectives.prefix());
        auto checked531 = m_root.imageMetadataDatabase();
if (!checked531) return std::unexpected(std::move(checked531.error()));
auto checked532 = Drawable::makeImage(*std::move(*checked531), std::move(image), 1.0f, true, personality.armOffset);
if (!checked532) return std::unexpected(std::move(checked532.error()));
Drawable drawable = std::move(*checked532);
        auto checked533 = m_root.imageMetadataDatabase();
if (!checked533) return std::unexpected(std::move(checked533.error()));
auto checked534 = drawable.imagePart().addDirectives(*std::move(*checked533), bodyDirectives, true);
if (!checked534) return std::unexpected(std::move(checked534.error()));
        auto checked535 = addDrawable(std::move(drawable));
if (!checked535) return std::unexpected(std::move(checked535.error()));
      }

      if (dressed) {
        for (std::uint8_t i : fashion.wornChestsLegs) {
          if (i == 0)
            break;
          auto const& wearable = fashion.wearables[std::size_t(i) - 1];
          if (std::holds_alternative<WornChest>(wearable) && !std::get<WornChest>(wearable).frontSleeveFrameset.empty()) {
            auto const& chest = std::get<WornChest>(wearable);
            String image = strf("{}:{}{}", chest.frontSleeveFrameset, personality.armIdle, chest.directives.prefix());
            auto checked536 = m_root.imageMetadataDatabase();
if (!checked536) return std::unexpected(std::move(checked536.error()));
auto checked537 = Drawable::makeImage(*std::move(*checked536), std::move(image), 1.0f, true, personality.armOffset);
if (!checked537) return std::unexpected(std::move(checked537.error()));
Drawable drawable = std::move(*checked537);
            auto checked538 = m_root.imageMetadataDatabase();
if (!checked538) return std::unexpected(std::move(checked538.error()));
auto checked539 = drawable.imagePart().addDirectives(*std::move(*checked538), chest.directives, true);
if (!checked539) return std::unexpected(std::move(checked539.error()));
            auto checked540 = addDrawable(std::move(drawable));
if (!checked540) return std::unexpected(std::move(checked540.error()));
          }
        }
      }
    }
  }

  return drawables;
}

std::expected<List<Drawable>, StarException> Humanoid::renderSkull() const {
  auto checked541 = m_root.imageMetadataDatabase();
if (!checked541) return std::unexpected(std::move(checked541.error()));
auto checked542 = m_root.speciesDatabase();
if (!checked542) return std::unexpected(std::move(checked542.error()));
auto checked543 = std::move(*checked542)->species(m_identity.species);
if (!checked543) return std::unexpected(std::move(checked543.error()));
auto checked544 = Drawable::makeImage(*std::move(*checked541),
      std::move(*checked543)->skull(), 1.0f, true, Vec2F());
if (!checked544) return std::unexpected(std::move(checked544.error()));
return List<Drawable>{std::move(*checked544)};
}

std::expected<std::shared_ptr<Humanoid>, StarException> Humanoid::makeDummy(Root& root, Gender) {
  auto checked545 = root.assets();
if (!checked545) return std::unexpected(std::move(checked545.error()));
auto assets = std::move(*checked545);
  auto checked562 = assets->json("/humanoid.config");
if (!checked562) return std::unexpected(std::move(checked562.error()));
auto checked546 = Humanoid::create(root, std::move(*checked562));
if (!checked546) return std::unexpected(std::move(checked546.error()));
std::shared_ptr<Humanoid> humanoid = std::move(*checked546);

  auto checked563 = assets->json("/humanoid/any/dummy.config:head");
if (!checked563) return std::unexpected(std::move(checked563.error()));
auto checked547 = std::move(*checked563).toString();
if (!checked547) return std::unexpected(std::move(checked547.error()));
humanoid->m_headFrameset = std::move(*checked547);
  auto checked564 = assets->json("/humanoid/any/dummy.config:body");
if (!checked564) return std::unexpected(std::move(checked564.error()));
auto checked548 = std::move(*checked564).toString();
if (!checked548) return std::unexpected(std::move(checked548.error()));
humanoid->m_bodyFrameset = std::move(*checked548);
  auto checked565 = assets->json("/humanoid/any/dummy.config:frontArm");
if (!checked565) return std::unexpected(std::move(checked565.error()));
auto checked549 = std::move(*checked565).toString();
if (!checked549) return std::unexpected(std::move(checked549.error()));
humanoid->m_frontArmFrameset = std::move(*checked549);
  auto checked566 = assets->json("/humanoid/any/dummy.config:backArm");
if (!checked566) return std::unexpected(std::move(checked566.error()));
auto checked550 = std::move(*checked566).toString();
if (!checked550) return std::unexpected(std::move(checked550.error()));
humanoid->m_backArmFrameset = std::move(*checked550);
auto checked567 = assets->json("/humanoid/any/dummy.config:direction");
if (!checked567) return std::unexpected(std::move(checked567.error()));
auto checked551 = std::move(*checked567).toString();
if (!checked551) return std::unexpected(std::move(checked551.error()));
auto checked552 = enumValue<DirectionNames>(std::move(*checked551));
if (!checked552) return std::unexpected(std::move(checked552.error()));
humanoid->setFacingDirection(*checked552);

return humanoid;
}

std::expected<List<Drawable>, StarException> Humanoid::renderDummy(Gender gender, std::optional<std::reference_wrapper<HeadArmor const>> head, std::optional<std::reference_wrapper<ChestArmor const>> chest, std::optional<std::reference_wrapper<LegsArmor const>> legs, std::optional<std::reference_wrapper<BackArmor const>> back) {
  std::shared_ptr<Fashion> fashion = std::move(m_fashion);
  auto state = m_state;
  m_state = State::Idle;
  float headRotation = m_headRotation;
  m_headRotation = 0.f;
  auto restore = [&]() {
    m_fashion = std::move(fashion);
    m_fashion->wornHeadsChanged = m_fashion->wornChestsLegsChanged = m_fashion->helmetMasksChanged = m_fashion->wornHeadsChanged = true;
    m_state = state;
    m_headRotation = headRotation;
  };
  auto result = [&]() -> std::expected<List<Drawable>, StarException> {
    List<Drawable> drawables;
    m_fashion = std::make_shared<Fashion>();
    if (head)
      {
auto checked553 = setWearableFromHead(3, head->get(), gender);
if (!checked553) return std::unexpected(std::move(checked553.error()));
}
    if (chest)
      {
auto checked554 = setWearableFromChest(2, chest->get(), gender);
if (!checked554) return std::unexpected(std::move(checked554.error()));
}
    if (legs)
      {
auto checked555 = setWearableFromLegs(1, legs->get(), gender);
if (!checked555) return std::unexpected(std::move(checked555.error()));
}
    if (back)
      {
auto checked556 = setWearableFromBack(0, back->get(), gender);
if (!checked556) return std::unexpected(std::move(checked556.error()));
}

    auto checked557 = render(false, false, 1.f);
if (!checked557) return std::unexpected(std::move(checked557.error()));
drawables = std::move(*checked557);
    Drawable::scaleAll(drawables, TilePixels);
    auto checked558 = removeWearable(0);
if (!checked558) return std::unexpected(std::move(checked558.error()));
    auto checked559 = removeWearable(1);
if (!checked559) return std::unexpected(std::move(checked559.error()));
    auto checked560 = removeWearable(2);
if (!checked560) return std::unexpected(std::move(checked560.error()));
    auto checked561 = removeWearable(3);
if (!checked561) return std::unexpected(std::move(checked561.error()));
    return drawables;
  }();
  restore();
  return result;
}

std::expected<Vec2F, StarException> Humanoid::primaryHandPosition(Vec2F const& offset) const {
  auto checked568 = primaryHandOffset(m_facingDirection);
if (!checked568) return std::unexpected(std::move(checked568.error()));
auto checked569 = primaryArmPosition(m_facingDirection, m_primaryHand.angle, std::move(*checked568) + offset);
if (!checked569) return std::unexpected(std::move(checked569.error()));
return std::move(*checked569);
}

std::expected<Vec2F, StarException> Humanoid::altHandPosition(Vec2F const& offset) const {
  auto checked570 = altHandOffset(m_facingDirection);
if (!checked570) return std::unexpected(std::move(checked570.error()));
auto checked571 = altArmPosition(m_facingDirection, m_altHand.angle, std::move(*checked570) + offset);
if (!checked571) return std::unexpected(std::move(checked571.error()));
return std::move(*checked571);
}

std::expected<Vec2F, StarException> Humanoid::primaryArmPosition(Direction facingDirection, float armAngle, Vec2F const& offset) const {
  if (m_useAnimation){
    // does the animator being configurable overcomplicate some things? yeah probably
    Vec2F rotationCenter;
    String anchor;
    StringList transformationGroups;
    String rotationTransformGroup;
    if (facingDirection == Direction::Left || m_twoHanded) {
      rotationTransformGroup = "frontArmRotation";
      auto checked572 = m_networkedAnimator->hasState("frontArm", m_primaryHand.frontFrame);
if (!checked572) return std::unexpected(std::move(checked572.error()));
auto state = std::move(*checked572) ? m_primaryHand.frontFrame : "rotation";
      // make sure we ge the properties for the arm rotation state
      auto checked573 = m_networkedAnimator->partProperty(m_frontArmRotationPoint.first,m_frontArmRotationPoint.second,{"frontArm"},state,1);
if (!checked573) return std::unexpected(std::move(checked573.error()));
auto checked574 = jsonToVec2F(std::move(*checked573));
if (!checked574) return std::unexpected(std::move(checked574.error()));
rotationCenter = std::move(*checked574);
      auto checked575 = m_networkedAnimator->partProperty(m_frontArmRotationPoint.first, "anchorPart", {"frontArm"},state,1);
if (!checked575) return std::unexpected(std::move(checked575.error()));
auto checked576 = std::move(*checked575).toString();
if (!checked576) return std::unexpected(std::move(checked576.error()));
anchor = std::move(*checked576);
      auto checked577 = m_networkedAnimator->partProperty(m_frontArmRotationPoint.first, "transformationGroups", {"frontArm"},state,1);
if (!checked577) return std::unexpected(std::move(checked577.error()));
auto checked578 = jsonToStringList(std::move(*checked577));
if (!checked578) return std::unexpected(std::move(checked578.error()));
transformationGroups = std::move(*checked578);
    } else {
      rotationTransformGroup = "backArmRotation";
      auto checked579 = m_networkedAnimator->hasState("backArm", m_primaryHand.backFrame);
if (!checked579) return std::unexpected(std::move(checked579.error()));
auto state = std::move(*checked579) ? m_primaryHand.backFrame : "rotation";
      // make sure we ge the properties for the arm rotation state
      auto checked580 = m_networkedAnimator->partProperty(m_backArmRotationPoint.first,m_backArmRotationPoint.second,{"backArm"},state,1);
if (!checked580) return std::unexpected(std::move(checked580.error()));
auto checked581 = jsonToVec2F(std::move(*checked580));
if (!checked581) return std::unexpected(std::move(checked581.error()));
rotationCenter = std::move(*checked581);
      auto checked582 = m_networkedAnimator->partProperty(m_backArmRotationPoint.first, "anchorPart", {"backArm"},state,1);
if (!checked582) return std::unexpected(std::move(checked582.error()));
auto checked583 = std::move(*checked582).toString();
if (!checked583) return std::unexpected(std::move(checked583.error()));
anchor = std::move(*checked583);
      auto checked584 = m_networkedAnimator->partProperty(m_backArmRotationPoint.first, "transformationGroups", {"backArm"},state,1);
if (!checked584) return std::unexpected(std::move(checked584.error()));
auto checked585 = jsonToStringList(std::move(*checked584));
if (!checked585) return std::unexpected(std::move(checked585.error()));
transformationGroups = std::move(*checked585);
    }
    // and now, we do the group transformations for the part, but where it would do the rotation, we do some finagling to do it here instead of
    // using the rotation stored in the animator
    // now I DOUBT people will be doing crazy things using the animation transformation properties on the arm rotation parts themselves
    // but rather parts that the arms are parented to with anchors, so I'm not accounting for that here
    auto mat = Mat3F::identity();
    auto i = transformationGroups.indexOf(rotationTransformGroup);
    if (i != NPos) {
      if (i > 0)
        {
auto checked586 = transformationGroups.slice(0, i-1);
if (!checked586) return std::unexpected(std::move(checked586.error()));
auto checked587 = m_networkedAnimator->groupTransformation(std::move(*checked586));
if (!checked587) return std::unexpected(std::move(checked587.error()));
mat = std::move(*checked587);
}
      auto rotated = Mat3F::identity();
      rotated.rotate(armAngle, rotationCenter);
      mat = rotated * mat;
      if (i < (transformationGroups.size() -1))
        {
auto checked588 = transformationGroups.slice(i+1, transformationGroups.size() -1);
if (!checked588) return std::unexpected(std::move(checked588.error()));
auto checked589 = m_networkedAnimator->groupTransformation(std::move(*checked588));
if (!checked589) return std::unexpected(std::move(checked589.error()));
mat = std::move(*checked589) * mat;
}
    } else {
      auto checked590 = m_networkedAnimator->groupTransformation(transformationGroups);
if (!checked590) return std::unexpected(std::move(checked590.error()));
mat = std::move(*checked590);
    }
    auto checked591 = m_networkedAnimator->partTransformation(anchor);
if (!checked591) return std::unexpected(std::move(checked591.error()));
mat = std::move(*checked591) * mat;
    auto position = mat.transformVec2(offset + rotationCenter);
    if (facingDirection == Direction::Left)
      position[0] *= -1;
    return position;
  }

  auto checked592 = getBobYOffset(1.f);
if (!checked592) return std::unexpected(std::move(checked592.error()));
float bobYOffset = std::move(*checked592);

  if (m_primaryHand.holdingItem) {
    Vec2F rotationCenter;
    if (facingDirection == Direction::Left || m_twoHanded)
      rotationCenter = m_frontArmRotationCenter + Vec2F(0, bobYOffset);
    else
      rotationCenter = m_backArmRotationCenter + Vec2F(0, bobYOffset) + m_backArmOffset;

    Vec2F position = offset.rotate(armAngle) + rotationCenter;

    if (facingDirection == Direction::Left)
      position[0] *= -1;

    return position;
  } else {
    // TODO: non-aiming item holding not supported yet
    return Vec2F();
  }
}

std::expected<Vec2F, StarException> Humanoid::altArmPosition(Direction facingDirection, float armAngle, Vec2F const& offset) const {
  if (m_useAnimation){
    // does the animator being configurable overcomplicate some things? yeah probably
    Vec2F rotationCenter;
    String anchor;
    StringList transformationGroups;
    String rotationTransformGroup;
    if (facingDirection == Direction::Right) {
      rotationTransformGroup = "frontArmRotation";
      auto checked593 = m_networkedAnimator->hasState("frontArm", m_primaryHand.frontFrame);
if (!checked593) return std::unexpected(std::move(checked593.error()));
auto state = std::move(*checked593) ? m_primaryHand.frontFrame : "rotation";
      // make sure we ge the properties for the arm rotation state
      auto checked594 = m_networkedAnimator->partProperty(m_frontArmRotationPoint.first,m_frontArmRotationPoint.second,{"frontArm"},state,1);
if (!checked594) return std::unexpected(std::move(checked594.error()));
auto checked595 = jsonToVec2F(std::move(*checked594));
if (!checked595) return std::unexpected(std::move(checked595.error()));
rotationCenter = std::move(*checked595);
      auto checked596 = m_networkedAnimator->partProperty(m_frontArmRotationPoint.first, "anchorPart", {"frontArm"},state,1);
if (!checked596) return std::unexpected(std::move(checked596.error()));
auto checked597 = std::move(*checked596).toString();
if (!checked597) return std::unexpected(std::move(checked597.error()));
anchor = std::move(*checked597);
      auto checked598 = m_networkedAnimator->partProperty(m_frontArmRotationPoint.first, "transformationGroups", {"frontArm"},state,1);
if (!checked598) return std::unexpected(std::move(checked598.error()));
auto checked599 = jsonToStringList(std::move(*checked598));
if (!checked599) return std::unexpected(std::move(checked599.error()));
transformationGroups = std::move(*checked599);
    } else {
      rotationTransformGroup = "backArmRotation";
      auto checked600 = m_networkedAnimator->hasState("backArm", m_primaryHand.backFrame);
if (!checked600) return std::unexpected(std::move(checked600.error()));
auto state = std::move(*checked600) ? m_primaryHand.backFrame : "rotation";
      // make sure we ge the properties for the arm rotation state
      auto checked601 = m_networkedAnimator->partProperty(m_backArmRotationPoint.first,m_backArmRotationPoint.second,{"backArm"},state,1);
if (!checked601) return std::unexpected(std::move(checked601.error()));
auto checked602 = jsonToVec2F(std::move(*checked601));
if (!checked602) return std::unexpected(std::move(checked602.error()));
rotationCenter = std::move(*checked602);
      auto checked603 = m_networkedAnimator->partProperty(m_backArmRotationPoint.first, "anchorPart", {"backArm"},state,1);
if (!checked603) return std::unexpected(std::move(checked603.error()));
auto checked604 = std::move(*checked603).toString();
if (!checked604) return std::unexpected(std::move(checked604.error()));
anchor = std::move(*checked604);
      auto checked605 = m_networkedAnimator->partProperty(m_backArmRotationPoint.first, "transformationGroups", {"backArm"},state,1);
if (!checked605) return std::unexpected(std::move(checked605.error()));
auto checked606 = jsonToStringList(std::move(*checked605));
if (!checked606) return std::unexpected(std::move(checked606.error()));
transformationGroups = std::move(*checked606);
    }
    // and now, we do the group transformations for the part, but where it would do the rotation, we do some finagling to do it here instead of
    // using the rotation stored in the animator
    // now I DOUBT people will be doing crazy things using the animation transformation properties on the arm rotation parts themselves
    // but rather parts that the arms are parented to with anchors, so I'm not accounting for that here
    auto mat = Mat3F::identity();
    auto i = transformationGroups.indexOf(rotationTransformGroup);
    if (i != NPos) {
      if (i > 0)
        {
auto checked607 = transformationGroups.slice(0, i-1);
if (!checked607) return std::unexpected(std::move(checked607.error()));
auto checked608 = m_networkedAnimator->groupTransformation(std::move(*checked607));
if (!checked608) return std::unexpected(std::move(checked608.error()));
mat = std::move(*checked608);
}
      auto rotated = Mat3F::identity();
      rotated.rotate(armAngle, rotationCenter);
      mat = rotated * mat;
      if (i < (transformationGroups.size() -1))
        {
auto checked609 = transformationGroups.slice(i+1, transformationGroups.size() -1);
if (!checked609) return std::unexpected(std::move(checked609.error()));
auto checked610 = m_networkedAnimator->groupTransformation(std::move(*checked609));
if (!checked610) return std::unexpected(std::move(checked610.error()));
mat = std::move(*checked610) * mat;
}
    } else {
      auto checked611 = m_networkedAnimator->groupTransformation(transformationGroups);
if (!checked611) return std::unexpected(std::move(checked611.error()));
mat = std::move(*checked611);
    }
    auto checked612 = m_networkedAnimator->partTransformation(anchor);
if (!checked612) return std::unexpected(std::move(checked612.error()));
mat = std::move(*checked612) * mat;
    auto position = mat.transformVec2(offset + rotationCenter);
    if (facingDirection == Direction::Left)
      position[0] *= -1;
    return position;
  }

  auto checked613 = getBobYOffset(1.f);
if (!checked613) return std::unexpected(std::move(checked613.error()));
float bobYOffset = std::move(*checked613);

  if (m_altHand.holdingItem) {
    Vec2F rotationCenter;
    if (facingDirection == Direction::Right)
      rotationCenter = m_frontArmRotationCenter + Vec2F(0, bobYOffset);
    else
      rotationCenter = m_backArmRotationCenter + Vec2F(0, bobYOffset) + m_backArmOffset;

    Vec2F position = Vec2F(offset).rotate(armAngle) + rotationCenter;

    if (facingDirection == Direction::Left)
      position[0] *= -1;

    return position;
  } else {
    // TODO: non-aiming item holding not supported yet
    return Vec2F();
  }
}

std::expected<Vec2F, StarException> Humanoid::primaryHandOffset(Direction facingDirection) const {
  if (m_useAnimation){
    if (facingDirection == Direction::Left || m_twoHanded)
      {
auto checked614 = m_networkedAnimator->partProperty(m_frontItemPart,"offset");
if (!checked614) return std::unexpected(std::move(checked614.error()));
auto checked615 = jsonToVec2F(std::move(*checked614));
if (!checked615) return std::unexpected(std::move(checked615.error()));
auto checked616 = m_networkedAnimator->partProperty(m_frontArmRotationPoint.first,m_frontArmRotationPoint.second);
if (!checked616) return std::unexpected(std::move(checked616.error()));
auto checked617 = jsonToVec2F(std::move(*checked616));
if (!checked617) return std::unexpected(std::move(checked617.error()));
return std::move(*checked615) - std::move(*checked617);
}
    else
      {
auto checked618 = m_networkedAnimator->partProperty(m_backItemPart,"offset");
if (!checked618) return std::unexpected(std::move(checked618.error()));
auto checked619 = jsonToVec2F(std::move(*checked618));
if (!checked619) return std::unexpected(std::move(checked619.error()));
auto checked620 = m_networkedAnimator->partProperty(m_backArmRotationPoint.first,m_backArmRotationPoint.second);
if (!checked620) return std::unexpected(std::move(checked620.error()));
auto checked621 = jsonToVec2F(std::move(*checked620));
if (!checked621) return std::unexpected(std::move(checked621.error()));
return std::move(*checked619) - std::move(*checked621);
}
  }
  if (facingDirection == Direction::Left || m_twoHanded)
    return m_frontHandPosition - m_frontArmRotationCenter;
  else
    return m_frontHandPosition - m_backArmRotationCenter;
}

std::expected<Vec2F, StarException> Humanoid::altHandOffset(Direction facingDirection) const {
  if (m_useAnimation){
    if (facingDirection == Direction::Left || m_twoHanded)
      {
auto checked622 = m_networkedAnimator->partProperty(m_backItemPart,"offset");
if (!checked622) return std::unexpected(std::move(checked622.error()));
auto checked623 = jsonToVec2F(std::move(*checked622));
if (!checked623) return std::unexpected(std::move(checked623.error()));
auto checked624 = m_networkedAnimator->partProperty(m_backArmRotationPoint.first,m_backArmRotationPoint.second);
if (!checked624) return std::unexpected(std::move(checked624.error()));
auto checked625 = jsonToVec2F(std::move(*checked624));
if (!checked625) return std::unexpected(std::move(checked625.error()));
return std::move(*checked623) - std::move(*checked625);
}
    else
      {
auto checked626 = m_networkedAnimator->partProperty(m_frontItemPart,"offset");
if (!checked626) return std::unexpected(std::move(checked626.error()));
auto checked627 = jsonToVec2F(std::move(*checked626));
if (!checked627) return std::unexpected(std::move(checked627.error()));
auto checked628 = m_networkedAnimator->partProperty(m_frontArmRotationPoint.first,m_frontArmRotationPoint.second);
if (!checked628) return std::unexpected(std::move(checked628.error()));
auto checked629 = jsonToVec2F(std::move(*checked628));
if (!checked629) return std::unexpected(std::move(checked629.error()));
return std::move(*checked627) - std::move(*checked629);
}
  }
  if (facingDirection == Direction::Left || m_twoHanded)
    return m_frontHandPosition - m_backArmRotationCenter;
  else
    return m_frontHandPosition - m_frontArmRotationCenter;
}

Humanoid::HandDrawingInfo const& Humanoid::getHand(ToolHand hand) const {
  return hand == ToolHand::Primary ? m_primaryHand : m_altHand;
}

std::expected<String, StarException> Humanoid::frameBase(State state) const {
  switch (state) {
    case State::Idle:
      return "idle";
    case State::Walk:
      return "walk";
    case State::Run:
      return "run";
    case State::Jump:
      return "jump";
    case State::Swim:
      return "swim";
    case State::SwimIdle:
      return "swimIdle";
    case State::Duck:
      return "duck";
    case State::Fall:
      return "fall";
    case State::Sit:
      return "sit";
    case State::Lay:
      return "lay";

    default:
      if (!StateNames) return std::unexpected(StateNames.error());
      {
        auto name = StateNames->getRight(state);
        if (!name) return std::unexpected(std::move(name.error()));
        return std::unexpected(StarException(strf("No such state '{}'", name->get())));
      }
  }
}

std::expected<String, StarException> Humanoid::emoteFrameBase(HumanoidEmote state) const {
  switch (state) {
    case HumanoidEmote::Idle:
      return "idle";
    case HumanoidEmote::Blabbering:
      return "blabber";
    case HumanoidEmote::Shouting:
      return "shout";
    case HumanoidEmote::Happy:
      return "happy";
    case HumanoidEmote::Sad:
      return "sad";
    case HumanoidEmote::NEUTRAL:
      return "neutral";
    case HumanoidEmote::Laugh:
      return "laugh";
    case HumanoidEmote::Annoyed:
      return "annoyed";
    case HumanoidEmote::Oh:
      return "oh";
    case HumanoidEmote::OOOH:
      return "oooh";
    case HumanoidEmote::Blink:
      return "blink";
    case HumanoidEmote::Wink:
      return "wink";
    case HumanoidEmote::Eat:
      return "eat";
    case HumanoidEmote::Sleep:
      return "sleep";

    default:
      if (!HumanoidEmoteNames) return std::unexpected(HumanoidEmoteNames.error());
      {
        auto name = HumanoidEmoteNames->getRight(state);
        if (!name) return std::unexpected(std::move(name.error()));
        return std::unexpected(StarException(strf("No such emote state '{}'", name->get())));
      }
  }
}

String Humanoid::getHeadFromIdentity() const {
  return strf("/humanoid/{}/{}head.png",
      m_identity.imagePath ? *m_identity.imagePath : m_identity.species,
      m_genderName);
}

String Humanoid::getBodyFromIdentity() const {
  return strf("/humanoid/{}/{}body.png",
      m_identity.imagePath ? *m_identity.imagePath : m_identity.species,
      m_genderName);
}

String Humanoid::getBodyMaskFromIdentity() const {
  return strf("/humanoid/{}/mask/{}body.png",
      m_identity.imagePath ? *m_identity.imagePath : m_identity.species,
      m_genderName);
}

String Humanoid::getBodyHeadMaskFromIdentity() const {
  return strf("/humanoid/{}/headmask/{}body.png",
      m_identity.imagePath ? *m_identity.imagePath : m_identity.species,
      m_genderName);
}

String Humanoid::getFacialEmotesFromIdentity() const {
  return strf("/humanoid/{}/emote.png", m_identity.imagePath ? *m_identity.imagePath : m_identity.species);
}

String Humanoid::getHairFromIdentity() const {
  if (m_identity.hairType.empty())
    return "";
  return strf("/humanoid/{}/{}/{}.png",
      m_identity.imagePath ? *m_identity.imagePath : m_identity.species,
      m_identity.hairGroup,
      m_identity.hairType);
}

String Humanoid::getFacialHairFromIdentity() const {
  if (m_identity.facialHairType.empty())
    return "";
  return strf("/humanoid/{}/{}/{}.png",
      m_identity.imagePath ? *m_identity.imagePath : m_identity.species,
      m_identity.facialHairGroup,
      m_identity.facialHairType);
}

String Humanoid::getFacialMaskFromIdentity() const {
  if (m_identity.facialMaskType.empty())
    return "";
  return strf("/humanoid/{}/{}/{}.png",
      m_identity.imagePath ? *m_identity.imagePath : m_identity.species,
      m_identity.facialMaskGroup,
      m_identity.facialMaskType);
}

String Humanoid::getBackArmFromIdentity() const {
  return strf("/humanoid/{}/backarm.png", m_identity.imagePath ? *m_identity.imagePath : m_identity.species);
}

String Humanoid::getFrontArmFromIdentity() const {
  return strf("/humanoid/{}/frontarm.png", m_identity.imagePath ? *m_identity.imagePath : m_identity.species);
}

String Humanoid::getVaporTrailFrameset() const {
  return "/humanoid/any/flames.png";
}

Directives const& Humanoid::getBodyDirectives() const {
  return m_identity.bodyDirectives;
}

Directives const& Humanoid::getHairDirectives() const {
  return m_identity.hairDirectives;
}

Directives const& Humanoid::getEmoteDirectives() const {
  return m_identity.emoteDirectives;
}

Directives const& Humanoid::getFacialHairDirectives() const {
  return m_identity.facialHairDirectives;
}

Directives const& Humanoid::getFacialMaskDirectives() const {
  return m_identity.facialMaskDirectives;
}

DirectivesGroup const& Humanoid::getHelmetMaskDirectivesGroup() const {
  return m_fashion->helmetMaskDirectivesGroup;
}

int Humanoid::getEmoteStateSequence(float interpolation) const {
  return m_timing.emoteStateSeq(lerp(interpolation, m_previousEmoteAnimationTimer, m_emoteAnimationTimer), m_emoteState);
}

std::expected<int, StarException> Humanoid::getArmStateSequence(float interpolation) const {
  int stateSeq = m_timing.stateSeq(lerp(interpolation, m_previousAnimationTimer, m_animationTimer), m_state);

  int armStateSeq;
  if (m_state == Walk) {
    auto checked630 = m_armWalkSeq.at(m_movingBackwards ? m_armWalkSeq.size() - stateSeq : stateSeq - 1);
if (!checked630) return std::unexpected(std::move(checked630.error()));
armStateSeq = checked630->get();
  } else if (m_state == Run) {
    auto checked631 = m_armRunSeq.at(m_movingBackwards ? m_armRunSeq.size() - stateSeq : stateSeq - 1);
if (!checked631) return std::unexpected(std::move(checked631.error()));
armStateSeq = checked631->get();
  } else {
    armStateSeq = stateSeq;
  }

  return armStateSeq;
}

int Humanoid::getBodyStateSequence(float interpolation) const {
  int stateSeq = m_timing.stateSeq(lerp(interpolation, m_previousAnimationTimer, m_animationTimer), m_state);

  int bodyStateSeq;
  if (m_movingBackwards && (m_state == Walk || m_state == Run))
    bodyStateSeq = m_timing.stateFrames[m_state] - stateSeq + 1;
  else
    bodyStateSeq = stateSeq;

  return bodyStateSeq;
}

std::expected<std::optional<std::shared_ptr<Dance>>, StarException> Humanoid::getDance() const {
  if (!m_dance.has_value())
    return {};

  auto checked632 = m_root.danceDatabase();
if (!checked632) return std::unexpected(std::move(checked632.error()));
auto danceDatabase = std::move(*checked632);
  auto checked633 = danceDatabase->getDance(*m_dance);
if (!checked633) return std::unexpected(std::move(checked633.error()));
return std::move(*checked633);
}

std::expected<void, StarException> Humanoid::refreshAnimationState(bool startNew) {
  if (m_movingBackwards)
    if (auto animationStates = m_animationStatesBackwards.maybe(m_state)) {
      for (auto args : *animationStates)
        {
auto checked634 = m_networkedAnimator->setLocalState(args.first, args.second.state, startNew || args.second.startNew, args.second.reverse);
if (!checked634) return std::unexpected(std::move(checked634.error()));
}
      return {};
    }
  if (auto animationStates = m_animationStates.maybe(m_state))
    for (auto args : *animationStates)
      {
auto checked635 = m_networkedAnimator->setLocalState(args.first, args.second.state, startNew || args.second.startNew, args.second.reverse);
if (!checked635) return std::unexpected(std::move(checked635.error()));
}
  return {};
}

std::expected<float, StarException> Humanoid::getBobYOffset(float interpolation) const {
  int bodyStateSeq = getBodyStateSequence(interpolation);
  float bobYOffset = 0.0f;
  if (m_state == Run) {
    auto checked636 = m_runBob.at(bodyStateSeq - 1);
if (!checked636) return std::unexpected(std::move(checked636.error()));
bobYOffset = m_runFallOffset + checked636->get();
  } else if (m_state == Fall) {
    bobYOffset = m_runFallOffset;
  } else if (m_state == Jump) {
    bobYOffset = m_jumpBob;
  } else if (m_state == Walk) {
    auto checked637 = m_walkBob.at(bodyStateSeq - 1);
if (!checked637) return std::unexpected(std::move(checked637.error()));
bobYOffset = checked637->get();
  } else if (m_state == Swim) {
    auto checked638 = m_swimBob.at(bodyStateSeq - 1);
if (!checked638) return std::unexpected(std::move(checked638.error()));
bobYOffset = checked638->get();
  } else if (m_state == Duck) {
    bobYOffset = m_duckOffset;
  } else if (m_state == Sit) {
    bobYOffset = m_sitOffset;
  } else if (m_state == Lay) {
    bobYOffset = m_layOffset;
  }
  return bobYOffset;
}

std::expected<Vec2F, StarException> Humanoid::armAdjustment() const {
  auto checked639 = getBobYOffset(1.f);
if (!checked639) return std::unexpected(std::move(checked639.error()));
return Vec2F(0, std::move(*checked639));
}

std::expected<Vec2F, StarException> Humanoid::mouthOffset(bool ignoreAdjustments) const {
  if (ignoreAdjustments) {
    return (m_mouthOffset).rotate(m_rotation);
  } else {
    if (m_useAnimation)
      {
auto checked640 = m_networkedAnimator->partPoint(m_mouthOffsetPoint.first, m_mouthOffsetPoint.second);
if (!checked640) return std::unexpected(std::move(checked640.error()));
return std::move(*checked640).value_or(m_mouthOffset).rotate(m_rotation);
}

    auto checked641 = getBobYOffset(1.f);
if (!checked641) return std::unexpected(std::move(checked641.error()));
Vec2F headPosition(0, std::move(*checked641));
    if (m_state == Idle)
      headPosition += m_identity.personality.headOffset / TilePixels;
    else if (m_state == Run)
      headPosition += m_headRunOffset;
    else if (m_state == Swim || m_state == SwimIdle)
      headPosition += m_headSwimOffset;
    else if (m_state == Duck)
      headPosition += m_headDuckOffset;
    else if (m_state == Sit)
      headPosition += m_headSitOffset;
    else if (m_state == Lay)
      headPosition += m_headLayOffset;

    return (m_mouthOffset + headPosition).rotate(m_rotation);
  }
}

std::expected<Vec2F, StarException> Humanoid::feetOffset() const {
  if (m_useAnimation)
    {
auto checked642 = m_networkedAnimator->partPoint(m_feetOffsetPoint.first, m_feetOffsetPoint.second);
if (!checked642) return std::unexpected(std::move(checked642.error()));
return std::move(*checked642).value_or(m_feetOffset).rotate(m_rotation);
}
  return m_feetOffset.rotate(m_rotation);
}

std::expected<Vec2F, StarException> Humanoid::headArmorOffset() const {
  if (m_useAnimation)
    {
auto checked643 = m_networkedAnimator->partPoint(m_headArmorOffsetPoint.first, m_headArmorOffsetPoint.second);
if (!checked643) return std::unexpected(std::move(checked643.error()));
return std::move(*checked643).value_or(m_headArmorOffset).rotate(m_rotation);
}

  auto checked644 = getBobYOffset(1.f);
if (!checked644) return std::unexpected(std::move(checked644.error()));
Vec2F headPosition(0, std::move(*checked644));
  if (m_state == Idle)
    headPosition += m_identity.personality.headOffset / TilePixels;
  else if (m_state == Run)
    headPosition += m_headRunOffset;
  else if (m_state == Swim || m_state == SwimIdle)
    headPosition += m_headSwimOffset;
  else if (m_state == Duck)
    headPosition += m_headDuckOffset;
  else if (m_state == Sit)
    headPosition += m_headSitOffset;
  else if (m_state == Lay)
    headPosition += m_headLayOffset;

  return (m_headArmorOffset + headPosition).rotate(m_rotation);
}

std::expected<Vec2F, StarException> Humanoid::chestArmorOffset() const {
  if (m_useAnimation)
    {
auto checked645 = m_networkedAnimator->partPoint(m_chestArmorOffsetPoint.first, m_chestArmorOffsetPoint.second);
if (!checked645) return std::unexpected(std::move(checked645.error()));
return std::move(*checked645).value_or(m_chestArmorOffset).rotate(m_rotation);
}

  auto checked646 = getBobYOffset(1.f);
if (!checked646) return std::unexpected(std::move(checked646.error()));
Vec2F position(0, std::move(*checked646));
  return (m_chestArmorOffset + position).rotate(m_rotation);
}

std::expected<Vec2F, StarException> Humanoid::legsArmorOffset() const {
  if (m_useAnimation)
    {
auto checked647 = m_networkedAnimator->partPoint(m_legsArmorOffsetPoint.first, m_legsArmorOffsetPoint.second);
if (!checked647) return std::unexpected(std::move(checked647.error()));
return std::move(*checked647).value_or(m_legsArmorOffset).rotate(m_rotation);
}
  return m_legsArmorOffset.rotate(m_rotation);
}

std::expected<Vec2F, StarException> Humanoid::backArmorOffset() const {
  if (m_useAnimation)
    {
auto checked648 = m_networkedAnimator->partPoint(m_backArmorOffsetPoint.first, m_backArmorOffsetPoint.second);
if (!checked648) return std::unexpected(std::move(checked648.error()));
return std::move(*checked648).value_or(m_backArmorOffset).rotate(m_rotation);
}
  auto checked649 = getBobYOffset(1.f);
if (!checked649) return std::unexpected(std::move(checked649.error()));
Vec2F position(0, std::move(*checked649));
  return (m_backArmorOffset + position).rotate(m_rotation);
}

String Humanoid::defaultDeathParticles() const {
  return m_defaultDeathParticles;
}

std::expected<List<Particle>, StarException> Humanoid::particles(String const& name) const {
  auto checked650 = m_root.particleDatabase();
if (!checked650) return std::unexpected(std::move(checked650.error()));
auto particleDatabase = std::move(*checked650);
  List<Particle> res;
  auto checked651 = m_particleEmitters.get(name);
if (!checked651) return std::unexpected(std::move(checked651.error()));
auto checked652 = std::move(*checked651).get("particles", {});
if (!checked652) return std::unexpected(std::move(checked652.error()));
Json particles = std::move(*checked652);
  {
auto checked653 = particles.toArray();
if (!checked653) return std::unexpected(std::move(checked653.error()));
for (auto& particle : std::move(*checked653)) {
    auto checked654 = particle.get("particle", {});
if (!checked654) return std::unexpected(std::move(checked654.error()));
auto particleSpec = std::move(*checked654);
    auto checked655 = particleDatabase->particle(particleSpec);
if (!checked655) return std::unexpected(std::move(checked655.error()));
res.push_back(std::move(*checked655));
  }
}

  return res;
}

Json const& Humanoid::defaultMovementParameters() const {
  return m_defaultMovementParameters;
}
std::optional<Json> const& Humanoid::playerMovementParameters() const {
  return m_playerMovementParameters;
}

std::expected<std::pair<Vec2F, Directives>, StarException> Humanoid::extractScaleFromDirectives(Directives const& directives) {
  if (!directives)
    return std::make_pair(Vec2F::filled(1.f), Directives());

  auto shared = directives.get();
  if (!shared) return std::unexpected(std::move(shared.error()));
  List<std::reference_wrapper<Directives::Entry const>> entries;
  std::size_t toReserve = 0;
  std::optional<Vec2F> scale;

  for (auto const& entry : shared->get().entries) {
    auto string = entry.string(shared->get());
    auto scaleNearest = string.beginsWith("scalenearest");
    if (!scaleNearest) return std::unexpected(std::move(scaleNearest.error()));
    if (*scaleNearest && string.utf8().find("skip") == NPos) {
      auto const& operation = entry.loadOperation(shared->get());
      if (std::holds_alternative<ScaleImageOperation>(operation)) {
        auto const& op = std::get<ScaleImageOperation>(operation);
        scale = scale.value_or(Vec2F::filled(1.f)).piecewiseMultiply(op.scale);
        continue;
      }
    }
    entries.emplace_back(std::cref(entry));
    toReserve += string.utf8Size() + 1;
  }

  if (!scale)
    return std::make_pair(Vec2F::filled(1.f), directives);

  String mergedDirectives;
  mergedDirectives.reserve(toReserve);
  for (auto const& entry : entries) {
    if (entry.get().begin > 0)
      mergedDirectives.append("?");
    mergedDirectives.append(entry.get().string(shared->get()));
  }

  return std::make_pair(*scale, Directives(mergedDirectives));
}

std::optional<std::reference_wrapper<NetworkedAnimator>> Humanoid::networkedAnimator() {
  return optionalRef(m_networkedAnimator);
}
std::optional<std::reference_wrapper<NetworkedAnimator const>> Humanoid::networkedAnimator() const {
  if (!m_networkedAnimator)
    return std::nullopt;
  return std::cref(*m_networkedAnimator);
}

List<String> Humanoid::animationScripts() const {
  return m_animationScripts;
}

std::expected<Json, StarException> Humanoid::humanoidConfig(bool withOverrides) {
  if (withOverrides)
    return jsonMerge(m_baseConfig, m_mergeConfig);
  return m_baseConfig;
}

NetHumanoid::NetHumanoid(Root& root) : m_root(root) {}

std::expected<std::shared_ptr<NetHumanoid>, StarException> NetHumanoid::create(Root& root) {
  return create(root, HumanoidIdentity(), JsonObject(), Json());
}

std::expected<std::shared_ptr<NetHumanoid>, StarException> NetHumanoid::create(Root& root, HumanoidIdentity identity, JsonObject parameters, Json config) {
  auto wrapper = std::shared_ptr<NetHumanoid>(new NetHumanoid(root));
  wrapper->m_config = config;
  wrapper->m_humanoidParameters.reset(parameters);
  auto humanoid = Humanoid::create(root, identity, parameters, config, std::nullopt);
  if (!humanoid) return std::unexpected(std::move(humanoid.error()));
  wrapper->m_humanoid = std::move(*humanoid);
  auto elements = wrapper->setupNetElements();
  if (!elements) return std::unexpected(std::move(elements.error()));
  return wrapper;
}

std::expected<void, StarException> NetHumanoid::netStore(DataStream& ds, NetCompatibilityRules rules) const {
  if (!checkWithRules(rules)) return {};
  auto fields = ds << m_humanoid->identity() << m_humanoidParameters.baseMap() << m_config;
  if (!fields) return std::unexpected(std::move(fields.error()));
  return NetElementSyncGroup::netStore(ds, rules);
}

std::expected<void, StarException> NetHumanoid::netLoad(DataStream& ds, NetCompatibilityRules rules) {
  if (!checkWithRules(rules)) return {};
  HumanoidIdentity identity;
  JsonObject parameters;
  Json config;
  auto fields = ds >> identity >> parameters >> config;
  if (!fields) return std::unexpected(std::move(fields.error()));
  auto humanoid = Humanoid::create(m_root, identity, parameters, config, std::nullopt);
  if (!humanoid) return std::unexpected(std::move(humanoid.error()));
  m_config = std::move(config);
  m_humanoidParameters.reset(std::move(parameters));
  m_humanoid = std::move(*humanoid);
  auto elements = setupNetElements();
  if (!elements) return std::unexpected(std::move(elements.error()));
  return NetElementSyncGroup::netLoad(ds, rules);
}

std::expected<void, StarException> NetHumanoid::netElementsNeedLoad(bool) {
  return {};
}

std::expected<void, StarException> NetHumanoid::netElementsNeedStore() {
  if (m_humanoidParameters.pullUpdated())
    return m_humanoid->setHumanoidParameters(m_humanoidParameters.baseMap());
  return {};
}

std::shared_ptr<Humanoid> NetHumanoid::humanoid() {
  return m_humanoid;
}

JsonObject NetHumanoid::humanoidParameters() {
  return m_humanoidParameters.baseMap();
}

std::expected<void, StarException> NetHumanoid::setHumanoidParameters(JsonObject parameters) {
  m_humanoidParameters.reset(parameters);
  return m_humanoid->setHumanoidParameters(std::move(parameters));
}

std::expected<void, StarException> NetHumanoid::setupNetElements() {
  clearNetElements();
  auto animator = addNetElement(m_humanoid->networkedAnimator()->get());
  if (!animator) return std::unexpected(std::move(animator.error()));
  return addNetElement(m_humanoidParameters);
}
}
