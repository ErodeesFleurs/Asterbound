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
import star.data_stream_extra;
import star.id_map;
import star.strong_typedef;
import star.casting;
import star.rpc_promise;
import star.a_star;

import star.drawable;
import star.image_metadata_database;
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

namespace {
using EmoteMap = EnumMap<HumanoidEmote>;

std::expected<EmoteMap, StarException> make_emote_map(bool frame_names) {
  EmoteMap result;
  for (auto const& description : emote_descriptions) {
    auto added = result.add({description.state, String(frame_names ? description.frame : description.name)});
    if (!added)
      return std::unexpected(std::move(added.error()));
  }
  return result;
}

auto const emote_frame_names = make_emote_map(true);
}

extern std::expected<EnumMap<HumanoidEmote>, StarException> const humanoid_emote_names = make_emote_map(false);

std::expected<Personality, StarException> parse_personality_array(Json const& config) {
  {
auto checked1 = config.get_string(0);
if (!checked1) return std::unexpected(std::move(checked1.error()));
auto checked2 = config.get_string(1);
if (!checked2) return std::unexpected(std::move(checked2.error()));
auto checked3 = config.get(2);
if (!checked3) return std::unexpected(std::move(checked3.error()));
auto checked4 = json_to_vec2f(std::move(*checked3));
if (!checked4) return std::unexpected(std::move(checked4.error()));
auto checked5 = config.get(3);
if (!checked5) return std::unexpected(std::move(checked5.error()));
auto checked6 = json_to_vec2f(std::move(*checked5));
if (!checked6) return std::unexpected(std::move(checked6.error()));
return Personality{std::move(*checked1), std::move(*checked2), std::move(*checked4), std::move(*checked6)};
}
}

std::expected<std::reference_wrapper<Personality>, StarException> parse_personality(Personality& personality, Json const& config) {
  {
auto checked7 = config.opt("idle");
if (!checked7) return std::unexpected(std::move(checked7.error()));
if (auto idle = std::move(*checked7))
    {
      auto idle_value = idle->to_string();
      if (!idle_value) return std::unexpected(std::move(idle_value.error()));
      personality.idle = std::move(*idle_value);
    }
}
  {
auto checked8 = config.opt("armIdle");
if (!checked8) return std::unexpected(std::move(checked8.error()));
if (auto arm_idle = std::move(*checked8))
    {
      auto arm_idle_value = arm_idle->to_string();
      if (!arm_idle_value) return std::unexpected(std::move(arm_idle_value.error()));
      personality.arm_idle = std::move(*arm_idle_value);
    }
}
  {
auto checked9 = config.opt("headOffset");
if (!checked9) return std::unexpected(std::move(checked9.error()));
if (auto head_offset = std::move(*checked9))
    {
auto checked10 = json_to_vec2f(*head_offset);
if (!checked10) return std::unexpected(std::move(checked10.error()));
personality.head_offset = std::move(*checked10);
}
}
  {
auto checked11 = config.opt("armOffset");
if (!checked11) return std::unexpected(std::move(checked11.error()));
if (auto arm_offset = std::move(*checked11))
    {
auto checked12 = json_to_vec2f(*arm_offset);
if (!checked12) return std::unexpected(std::move(checked12.error()));
personality.arm_offset = std::move(*checked12);
}
}

  return std::ref(personality);
}

std::expected<Personality, StarException> parse_personality(Json const& config) {
  Personality personality;
  auto parsed = parse_personality(personality, config);
  if (!parsed) return std::unexpected(std::move(parsed.error()));
  return personality;
}

Json json_from_personality(Personality const& personality) {
  return JsonObject{
    { "idle", personality.idle },
    { "armIdle", personality.arm_idle },
    { "headOffset", json_from_vec2f(personality.head_offset) },
    { "armOffset", json_from_vec2f(personality.arm_offset)   }
  };
}

HumanoidIdentity::HumanoidIdentity()
  : name("Humanoid"), species("human"), gender(Gender::Male), hair_group("hair"), hair_type("male1"), color(51, 117, 237, 255) {}

std::expected<HumanoidIdentity, StarException> HumanoidIdentity::create(Json config) {
  HumanoidIdentity identity;
  if (config.is_null())
    config = JsonObject();

  {
auto checked13 = config.get_string("name", "Humanoid");
if (!checked13) return std::unexpected(std::move(checked13.error()));
identity.name = std::move(*checked13);
}
  {
auto checked14 = config.get_string("species", "human");
if (!checked14) return std::unexpected(std::move(checked14.error()));
identity.species = std::move(*checked14);
}
  {
auto checked15 = config.get_string("gender", "male");
if (!checked15) return std::unexpected(std::move(checked15.error()));
auto checked16 = enum_value<gender_names>(*checked15);
if (!checked16) return std::unexpected(std::move(checked16.error()));
identity.gender = *checked16;
}
  {
auto checked17 = config.get_string("hairGroup", "hair");
if (!checked17) return std::unexpected(std::move(checked17.error()));
identity.hair_group = std::move(*checked17);
}
  {
auto checked18 = config.get_string("hairType", "male1");
if (!checked18) return std::unexpected(std::move(checked18.error()));
identity.hair_type = std::move(*checked18);
}
  {
auto checked19 = config.get_string("hairDirectives", "");
if (!checked19) return std::unexpected(std::move(checked19.error()));
identity.hair_directives = std::move(*checked19);
}
  {
auto checked20 = config.get_string("bodyDirectives", "");
if (!checked20) return std::unexpected(std::move(checked20.error()));
identity.body_directives = std::move(*checked20);
}
  {
auto checked21 = config.opt_string("emoteDirectives");
if (!checked21) return std::unexpected(std::move(checked21.error()));
if (auto j_emote_directives = std::move(*checked21)) // Passing Directives as a default arg would be inefficient
    {
auto checked22 = take_optional(j_emote_directives);
if (!checked22) return std::unexpected(std::move(checked22.error()));
identity.emote_directives = std::move(*checked22);
}
  else
    identity.emote_directives = identity.body_directives;
}

  {
auto checked23 = config.get_string("facialHairGroup", "");
if (!checked23) return std::unexpected(std::move(checked23.error()));
identity.facial_hair_group = std::move(*checked23);
}
  {
auto checked24 = config.get_string("facialHairType", "");
if (!checked24) return std::unexpected(std::move(checked24.error()));
identity.facial_hair_type = std::move(*checked24);
}
  {
auto checked25 = config.get_string("facialHairDirectives", "");
if (!checked25) return std::unexpected(std::move(checked25.error()));
identity.facial_hair_directives = std::move(*checked25);
}
  {
auto checked26 = config.get_string("facialMaskGroup", "");
if (!checked26) return std::unexpected(std::move(checked26.error()));
identity.facial_mask_group = std::move(*checked26);
}
  {
auto checked27 = config.get_string("facialMaskType", "");
if (!checked27) return std::unexpected(std::move(checked27.error()));
identity.facial_mask_type = std::move(*checked27);
}
  {
auto checked28 = config.get_string("facialMaskDirectives", "");
if (!checked28) return std::unexpected(std::move(checked28.error()));
identity.facial_mask_directives = std::move(*checked28);
}

  {
auto checked29 = config.get_string("personalityIdle", "idle.1");
if (!checked29) return std::unexpected(std::move(checked29.error()));
identity.personality.idle = std::move(*checked29);
}
  {
auto checked30 = config.get_string("personalityArmIdle", "idle.1");
if (!checked30) return std::unexpected(std::move(checked30.error()));
identity.personality.arm_idle = std::move(*checked30);
}
  {
auto checked31 = config.get("personalityHeadOffset", JsonArray{0, 0});
if (!checked31) return std::unexpected(std::move(checked31.error()));
auto checked32 = json_to_vec2f(std::move(*checked31));
if (!checked32) return std::unexpected(std::move(checked32.error()));
identity.personality.head_offset = std::move(*checked32);
}
  {
auto checked33 = config.get("personalityArmOffset", JsonArray{0, 0});
if (!checked33) return std::unexpected(std::move(checked33.error()));
auto checked34 = json_to_vec2f(std::move(*checked33));
if (!checked34) return std::unexpected(std::move(checked34.error()));
identity.personality.arm_offset = std::move(*checked34);
}

  {
auto checked35 = config.get("color", JsonArray({51, 117, 237, 255}));
if (!checked35) return std::unexpected(std::move(checked35.error()));
auto checked36 = json_to_color(std::move(*checked35));
if (!checked36) return std::unexpected(std::move(checked36.error()));
identity.color = checked36->to_rgba();
}

  {
auto checked38 = config.opt_string("imagePath");
if (!checked38) return std::unexpected(std::move(checked38.error()));
identity.image_path = std::move(*checked38);
}

  return identity;
}

std::expected<Json, StarException> HumanoidIdentity::to_json() const {
  auto checked39 = enum_name<gender_names>(gender);
  if (!checked39)
    return std::unexpected(std::move(checked39.error()));
  auto result = JsonObject{
    {"name", name},
    {"species", species},
    {"gender", String(*checked39)},
    {"hairGroup", hair_group},
    {"hairType", hair_type},
    {"hairDirectives", hair_directives.string()},
    {"bodyDirectives", body_directives.string()},
    {"emoteDirectives", emote_directives.string()},
    {"facialHairGroup", facial_hair_group},
    {"facialHairType", facial_hair_type},
    {"facialHairDirectives", facial_hair_directives.string()},
    {"facialMaskGroup", facial_mask_group},
    {"facialMaskType", facial_mask_type},
    {"facialMaskDirectives", facial_mask_directives.string()},
    {"personalityIdle", personality.idle},
    {"personalityArmIdle", personality.arm_idle},
    {"personalityHeadOffset", json_from_vec2f(personality.head_offset)},
    {"personalityArmOffset", json_from_vec2f(personality.arm_offset)},
    {"color", json_from_color(Color::rgba(color))},
  };
  if (image_path)
    result["imagePath"] = *image_path;
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
auto checked43 = ds.read(identity.hair_group);
if (!checked43) return std::unexpected(std::move(checked43.error()));
}
  {
auto checked44 = ds.read(identity.hair_type);
if (!checked44) return std::unexpected(std::move(checked44.error()));
}
  {
auto checked45 = ds.read(identity.hair_directives);
if (!checked45) return std::unexpected(std::move(checked45.error()));
}
  {
auto checked46 = ds.read(identity.body_directives);
if (!checked46) return std::unexpected(std::move(checked46.error()));
}
  {
auto checked47 = ds.read(identity.emote_directives);
if (!checked47) return std::unexpected(std::move(checked47.error()));
}
  {
auto checked48 = ds.read(identity.facial_hair_group);
if (!checked48) return std::unexpected(std::move(checked48.error()));
}
  {
auto checked49 = ds.read(identity.facial_hair_type);
if (!checked49) return std::unexpected(std::move(checked49.error()));
}
  {
auto checked50 = ds.read(identity.facial_hair_directives);
if (!checked50) return std::unexpected(std::move(checked50.error()));
}
  {
auto checked51 = ds.read(identity.facial_mask_group);
if (!checked51) return std::unexpected(std::move(checked51.error()));
}
  {
auto checked52 = ds.read(identity.facial_mask_type);
if (!checked52) return std::unexpected(std::move(checked52.error()));
}
  {
auto checked53 = ds.read(identity.facial_mask_directives);
if (!checked53) return std::unexpected(std::move(checked53.error()));
}
  {
auto checked54 = ds.read(identity.personality.idle);
if (!checked54) return std::unexpected(std::move(checked54.error()));
}
  {
auto checked55 = ds.read(identity.personality.arm_idle);
if (!checked55) return std::unexpected(std::move(checked55.error()));
}
  {
auto checked56 = ds.read(identity.personality.head_offset);
if (!checked56) return std::unexpected(std::move(checked56.error()));
}
  {
auto checked57 = ds.read(identity.personality.arm_offset);
if (!checked57) return std::unexpected(std::move(checked57.error()));
}
  {
auto checked58 = ds.read(identity.color);
if (!checked58) return std::unexpected(std::move(checked58.error()));
}
  {
auto checked59 = ds.read(identity.image_path);
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
auto checked63 = ds.write(identity.hair_group);
if (!checked63) return std::unexpected(std::move(checked63.error()));
}
  {
auto checked64 = ds.write(identity.hair_type);
if (!checked64) return std::unexpected(std::move(checked64.error()));
}
  {
auto checked65 = ds.write(identity.hair_directives);
if (!checked65) return std::unexpected(std::move(checked65.error()));
}
  {
auto checked66 = ds.write(identity.body_directives);
if (!checked66) return std::unexpected(std::move(checked66.error()));
}
  {
auto checked67 = ds.write(identity.emote_directives);
if (!checked67) return std::unexpected(std::move(checked67.error()));
}
  {
auto checked68 = ds.write(identity.facial_hair_group);
if (!checked68) return std::unexpected(std::move(checked68.error()));
}
  {
auto checked69 = ds.write(identity.facial_hair_type);
if (!checked69) return std::unexpected(std::move(checked69.error()));
}
  {
auto checked70 = ds.write(identity.facial_hair_directives);
if (!checked70) return std::unexpected(std::move(checked70.error()));
}
  {
auto checked71 = ds.write(identity.facial_mask_group);
if (!checked71) return std::unexpected(std::move(checked71.error()));
}
  {
auto checked72 = ds.write(identity.facial_mask_type);
if (!checked72) return std::unexpected(std::move(checked72.error()));
}
  {
auto checked73 = ds.write(identity.facial_mask_directives);
if (!checked73) return std::unexpected(std::move(checked73.error()));
}
  {
auto checked74 = ds.write(identity.personality.idle);
if (!checked74) return std::unexpected(std::move(checked74.error()));
}
  {
auto checked75 = ds.write(identity.personality.arm_idle);
if (!checked75) return std::unexpected(std::move(checked75.error()));
}
  {
auto checked76 = ds.write(identity.personality.head_offset);
if (!checked76) return std::unexpected(std::move(checked76.error()));
}
  {
auto checked77 = ds.write(identity.personality.arm_offset);
if (!checked77) return std::unexpected(std::move(checked77.error()));
}
  {
auto checked78 = ds.write(identity.color);
if (!checked78) return std::unexpected(std::move(checked78.error()));
}
  {
auto checked79 = ds.write(identity.image_path);
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
    auto config_result = assets->json("/humanoid.config:humanoidTiming");
    if (!config_result) return std::unexpected(std::move(config_result.error()));
    config = std::move(*config_result);
  }

  {
auto checked81 = config.contains("stateCycle");
if (!checked81) return std::unexpected(std::move(checked81.error()));
if (std::move(*checked81))
    {
auto checked82 = config.get("stateCycle");
if (!checked82) return std::unexpected(std::move(checked82.error()));
auto checked83 = json_to_array_f<state_size>(std::move(*checked82));
if (!checked83) return std::unexpected(std::move(checked83.error()));
timing.state_cycle = std::move(*checked83);
}
}
  {
auto checked84 = config.contains("stateFrames");
if (!checked84) return std::unexpected(std::move(checked84.error()));
if (std::move(*checked84))
    {
auto checked85 = config.get("stateFrames");
if (!checked85) return std::unexpected(std::move(checked85.error()));
auto checked86 = json_to_array_u<state_size>(std::move(*checked85));
if (!checked86) return std::unexpected(std::move(checked86.error()));
timing.state_frames = std::move(*checked86);
}
}

  auto cycles = config.get("emoteCycle");
  if (!cycles)
    return std::unexpected(std::move(cycles.error()));
  auto frames = config.get("emoteFrames");
  if (!frames)
    return std::unexpected(std::move(frames.error()));
  auto cycle_values = json_to_array_d<emote_size>(*cycles);
  if (!cycle_values)
    return std::unexpected(std::move(cycle_values.error()));
  auto frame_values = json_to_array_d<emote_size>(*frames);
  if (!frame_values)
    return std::unexpected(std::move(frame_values.error()));
  for (std::size_t i = 0; i < emote_size; ++i) {
    auto cycle = (*cycle_values)[i];
    if (!is_finite(cycle) || cycle <= 0 || cycle > std::numeric_limits<float>::max()
        || static_cast<float>(cycle) == 0)
      return std::unexpected(StarException(Star::format("Invalid emoteCycle for {}", emote_descriptions[i].name)));
    auto count = (*frame_values)[i];
    if (!is_finite(count) || count < 1 || count > std::numeric_limits<int>::max() || std::trunc(count) != count)
      return std::unexpected(StarException(Star::format("Invalid emoteFrames for {}", emote_descriptions[i].name)));
    timing.emote_cycle[i] = static_cast<float>(cycle);
    timing.emote_frames[i] = static_cast<unsigned>(count);
  }
  return timing;
}

bool Humanoid::HumanoidTiming::cyclic_state(State state) {
  switch (state) {
    case State::Walk:
    case State::Run:
    case State::Swim:
      return true;
    default:
      return false;
  }
}

bool Humanoid::HumanoidTiming::cyclic_emote_state(HumanoidEmote state) {
  auto index = static_cast<std::size_t>(state);
  return index < emote_size && emote_descriptions[index].cyclic;
}

int Humanoid::HumanoidTiming::state_seq(float timer, State state) const {
  return generic_seq(timer, state_cycle[state], state_frames[state], cyclic_state(state));
}

int Humanoid::HumanoidTiming::emote_state_seq(float timer, HumanoidEmote state) const {
  auto index = static_cast<std::size_t>(state);
  return generic_seq(timer, emote_cycle[index], emote_frames[index], emote_descriptions[index].cyclic);
}

int Humanoid::HumanoidTiming::generic_seq(float timer, float cycle, unsigned frames, bool cyclic) const {
  if (frames <= 1 || timer <= 0)
    return 1;
  auto last = static_cast<int>(std::min(frames, static_cast<unsigned>(std::numeric_limits<int>::max())));
  if (timer >= cycle) {
    if (!cyclic)
      return last;
    timer = std::fmod(timer, cycle);
  }
  // Preserve the original float rounding for ordinary configurations. Widen
  // only when the numerator could overflow before division brings it back.
  if (timer >= 0x1p95f) [[unlikely]] {
    double frame = static_cast<double>(timer) * frames / cycle;
    if (!is_finite(frame) || frame <= 0)
      return 1;
    return frame >= last - 1 ? last : static_cast<int>(frame) + 1;
  }
  float frame = timer * frames / cycle;
  // For nonnegative IEEE floats the bits are ordered by magnitude. One
  // comparison excludes negatives, nonfinite values and the int overflow
  // boundary (INT_MAX itself rounds up as a float).
  auto bits = std::bit_cast<std::uint32_t>(frame);
  if (bits >= std::bit_cast<std::uint32_t>(0x1p31f)) [[unlikely]]
    return bits < std::bit_cast<std::uint32_t>(std::numeric_limits<float>::infinity()) ? last : 1;
  return std::min(static_cast<int>(frame), last - 1) + 1;
}

std::expected<EnumMap<Humanoid::State>, StarException> const Humanoid::state_names = EnumMap<Humanoid::State>::create({
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

Humanoid::Humanoid(Root& root) : m_root(root) {
  m_fashion = std::make_shared<Fashion>();

  m_two_handed = false;
  m_primary_hand.holding_item = false;
  m_alt_hand.holding_item = false;

  m_moving_backwards = false;
  m_alt_hand.angle = 0;
  m_facing_direction = Direction::Left;
  m_head_rotation_target = m_head_rotation = m_rotation = 0;
  m_scale = Vec2F::filled(1.f);
  m_draw_vapor_trail = false;
  m_state = State::Idle;
  m_emote_state = HumanoidEmote::Idle;

  m_primary_hand.angle = 0;
}

std::expected<std::shared_ptr<Humanoid>, StarException> Humanoid::create(Root& root) {
  auto humanoid = std::shared_ptr<Humanoid>(new Humanoid(root));
  auto timing = HumanoidTiming::create(root, {});
  if (!timing) return std::unexpected(std::move(timing.error()));
  humanoid->m_config.timing = std::move(*timing);
  auto animator = NetworkedAnimator::create(root);
  if (!animator) return std::unexpected(std::move(animator.error()));
  humanoid->m_networked_animator = std::move(*animator);
  return humanoid;
}

std::expected<std::shared_ptr<Humanoid>, StarException> Humanoid::create(Humanoid const& source) {
  auto animator = NetworkedAnimator::create(*source.m_networked_animator);
  if (!animator) return std::unexpected(std::move(animator.error()));
  auto humanoid = std::shared_ptr<Humanoid>(new Humanoid(source));
  humanoid->m_networked_animator = std::move(*animator);
  return humanoid;
}

std::expected<std::shared_ptr<Humanoid>, StarException> Humanoid::create(Root& root, Json const& config) {
  auto humanoid = create(root);
  if (!humanoid) return std::unexpected(std::move(humanoid.error()));
  (*humanoid)->m_base_config = config;
  auto loaded = (*humanoid)->load_config(JsonObject());
  if (!loaded) return std::unexpected(std::move(loaded.error()));
  auto animated = (*humanoid)->load_animation();
  if (!animated) return std::unexpected(std::move(animated.error()));
  return humanoid;
}

std::expected<std::shared_ptr<Humanoid>, StarException> Humanoid::create(Root& root, HumanoidIdentity const& identity, JsonObject parameters, Json config, std::optional<std::reference_wrapper<ItemDatabase const>> item_catalog) {
  auto humanoid = create(root);
  if (!humanoid) return std::unexpected(std::move(humanoid.error()));
  auto database = root.speciesDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  auto base = (*database)->humanoid_config(identity, std::move(parameters), std::move(config), item_catalog);
  if (!base) return std::unexpected(std::move(base.error()));
  (*humanoid)->m_identity = identity;
  (*humanoid)->m_base_config = std::move(*base);
  auto loaded = (*humanoid)->load_config(JsonObject());
  if (!loaded) return std::unexpected(std::move(loaded.error()));
  auto animated = (*humanoid)->load_animation();
  if (!animated) return std::unexpected(std::move(animated.error()));
  auto identified = (*humanoid)->set_identity(identity);
  if (!identified) return std::unexpected(std::move(identified.error()));
  return humanoid;
}

std::expected<void, StarException> Humanoid::set_identity(HumanoidIdentity const& identity) {
  auto checked93 = enum_name<gender_names>(identity.gender);
  if (!checked93)
    return std::unexpected(std::move(checked93.error()));
  m_gender_name = String(*checked93);
  m_identity = identity;
  m_head_frameset = get_head_from_identity();
  m_body_frameset = get_body_from_identity();
  m_emote_frameset = get_facial_emotes_from_identity();
  m_hair_frameset = get_hair_from_identity();
  m_facial_hair_frameset = get_facial_hair_from_identity();
  m_facial_mask_frameset = get_facial_mask_from_identity();
  m_back_arm_frameset = get_back_arm_from_identity();
  m_front_arm_frameset = get_front_arm_from_identity();
  m_vapor_trail_frameset = get_vapor_trail_frameset();
  if (m_config.use_body_mask) {
    m_body_mask_frameset = get_body_mask_from_identity();
    
}
  if (m_config.use_body_head_mask) {
    m_body_head_mask_frameset = get_body_head_mask_from_identity();
    
}
  if (m_use_animation) {
    m_networked_animator->set_local_tag("name", m_identity.name);
    m_networked_animator->set_local_tag("species", m_identity.species);
    m_networked_animator->set_local_tag("gender", m_gender_name);
    m_networked_animator->set_local_tag("hairGroup", m_identity.hair_group);
    m_networked_animator->set_local_tag("hairType", m_identity.hair_type);
    m_networked_animator->set_local_tag("hairDirectives", m_identity.hair_directives.string());
    m_networked_animator->set_local_tag("bodyDirectives", m_identity.body_directives.string());
    m_networked_animator->set_local_tag("emoteDirectives", m_identity.emote_directives.string());
    m_networked_animator->set_local_tag("facialHairGroup", m_identity.facial_hair_group);
    m_networked_animator->set_local_tag("facialHairType", m_identity.facial_hair_type);
    m_networked_animator->set_local_tag("facialHairDirectives", m_identity.facial_hair_directives.string());
    m_networked_animator->set_local_tag("facialMaskGroup", m_identity.facial_hair_group);
    m_networked_animator->set_local_tag("facialMaskType", m_identity.facial_hair_type);
    m_networked_animator->set_local_tag("facialMaskDirectives", m_identity.facial_mask_directives.string());
    m_networked_animator->set_local_tag("personalityIdle", m_identity.personality.idle);
    m_networked_animator->set_local_tag("personalityArmIdle", m_identity.personality.arm_idle);
    auto checked94 = m_networked_animator->reset_local_transformation_group("personalityHeadOffset");
if (!checked94) return std::unexpected(std::move(checked94.error()));
    auto checked95 = m_networked_animator->translate_local_transformation_group("personalityHeadOffset", m_identity.personality.head_offset / tile_pixels);
if (!checked95) return std::unexpected(std::move(checked95.error()));
    auto checked96 = m_networked_animator->reset_local_transformation_group("personalityArmOffset");
if (!checked96) return std::unexpected(std::move(checked96.error()));
    auto checked97 = m_networked_animator->translate_local_transformation_group("personalityArmOffset", m_identity.personality.arm_offset / tile_pixels);
if (!checked97) return std::unexpected(std::move(checked97.error()));

m_networked_animator->set_local_tag(
  "hairFrameset",
  m_identity.hair_type.empty() ? "" : String(Star::format("{}/{}.png", m_identity.hair_group, m_identity.hair_type)));
m_networked_animator->set_local_tag("facialHairFrameset",
                                 m_identity.facial_hair_type.empty()
                                   ? ""
                                   : String(Star::format("{}/{}.png", m_identity.facial_hair_group, m_identity.facial_hair_type)));
m_networked_animator->set_local_tag("facialMaskFrameset",
                                 m_identity.facial_mask_type.empty()
                                   ? ""
                                   : String(Star::format("{}/{}.png", m_identity.facial_mask_group, m_identity.facial_mask_type)));

for (auto const& p : m_identity_frameset_tags) {
  auto checked98 = m_networked_animator->apply_part_tags("anchor", p.first);
  if (!checked98)
    return std::unexpected(std::move(checked98.error()));
  auto checked99 = m_networked_animator->apply_part_tags("anchor", p.second);
  if (!checked99)
    return std::unexpected(std::move(checked99.error()));
  m_networked_animator->set_local_tag(std::move(*checked98), std::move(*checked99));
}
}
  return {};
}

HumanoidIdentity const& Humanoid::identity() const {
  return m_identity;
}

std::expected<void, StarException> Humanoid::set_humanoid_parameters(JsonObject parameters) {
  auto base = json_merge(m_base_config, std::move(parameters));
  auto applied = apply_config(json_merge(base, m_merge_config));
  if (!applied)
    return std::unexpected(std::move(applied.error()));
  m_base_config = std::move(base);
  return {};
}

std::expected<bool, StarException> Humanoid::load_config(Json merger, std::optional<bool> unchanged_body_hidden) {
  if (m_merge_config == merger) {
    // Armor can change its hide-body flag without changing its config overlay.
    if (unchanged_body_hidden)
      m_config.body_hidden = *unchanged_body_hidden;
    return false;
  }
  auto applied = apply_config(json_merge(m_base_config, merger));
  if (!applied)
    return std::unexpected(std::move(applied.error()));
  m_merge_config = std::move(merger);
  return *applied;
}

std::expected<bool, StarException> Humanoid::apply_config(Json const& config) {
  auto timing_config = config.get("humanoidTiming");
  if (!timing_config)
    return std::unexpected(std::move(timing_config.error()));
  // Reject non-objects here instead of letting HumanoidTiming fall back to root assets.
  if (auto object = timing_config->object_ptr(); !object)
    return std::unexpected(std::move(object.error()));
  auto timing = HumanoidTiming::create(m_root, std::move(*timing_config));
  if (!timing)
    return std::unexpected(std::move(timing.error()));

  auto pixel_vector = [](Json const& value) {
    return json_to_vec2f(value).transform([](Vec2F offset) { return offset / tile_pixels; });
  };
  auto offset = [&](String const& key) { return config.get(key).and_then(pixel_vector); };
  auto scalar = [&](String const& key) {
    return config.get_double(key).transform([](double pixels) { return static_cast<float>(pixels / tile_pixels); });
  };
  auto pixels_list = [&](String const& key) -> std::expected<List<float>, StarException> {
    auto array = config.get(key).and_then(&Json::array_ptr);
    if (!array)
      return std::unexpected(std::move(array.error()));
    List<float> values;
    values.reserve((*array)->size());
    for (auto const& element : **array) {
      auto pixels = element.to_double();
      if (!pixels)
        return std::unexpected(std::move(pixels.error()));
      values.push_back(static_cast<float>(*pixels / tile_pixels));
    }
    return values;
  };

  Config next;
  next.timing = std::move(*timing);
  std::optional<StarException> error;
  auto read = [&error]<typename T, typename U>(T& field, std::expected<U, StarException> value)
      requires std::assignable_from<T&, U> {
    if (!value) {
      error.emplace(std::move(value.error()));
      return false;
    }
    field = std::move(*value);
    return true;
  };
  if (!read(next.global_offset, offset("globalOffset"))
      || !read(next.head_run_offset, offset("headRunOffset"))
      || !read(next.head_swim_offset, offset("headSwimOffset"))
      || !read(next.run_fall_offset, scalar("runFallOffset"))
      || !read(next.duck_offset, scalar("duckOffset"))
      || !read(next.head_duck_offset, offset("headDuckOffset"))
      || !read(next.sit_offset, scalar("sitOffset"))
      || !read(next.lay_offset, scalar("layOffset"))
      || !read(next.head_sit_offset, offset("headSitOffset"))
      || !read(next.head_lay_offset, offset("headLayOffset"))
      || !read(next.recoil_offset, offset("recoilOffset"))
      || !read(next.mouth_offset, offset("mouthOffset"))
      || !read(next.feet_offset, offset("feetOffset"))
      || !read(next.body_fullbright, config.get_bool("bodyFullbright", false))
      || !read(next.use_body_mask, config.get_bool("useBodyMask", false))
      || !read(next.use_body_head_mask, config.get_bool("useBodyHeadMask", false))
      || !read(next.head_armor_offset, offset("headArmorOffset"))
      || !read(next.chest_armor_offset, offset("chestArmorOffset"))
      || !read(next.legs_armor_offset, offset("legsArmorOffset"))
      || !read(next.back_armor_offset, offset("backArmorOffset"))
      || !read(next.body_hidden, config.get_bool("bodyHidden", false))
      || !read(next.arm_walk_seq, config.get("armWalkSeq").and_then(json_to_int_list))
      || !read(next.arm_run_seq, config.get("armRunSeq").and_then(json_to_int_list))
      || !read(next.walk_bob, pixels_list("walkBob"))
      || !read(next.run_bob, pixels_list("runBob"))
      || !read(next.swim_bob, pixels_list("swimBob"))
      || !read(next.jump_bob, scalar("jumpBob"))
      || !read(next.front_arm_rotation_center, offset("frontArmRotationCenter"))
      || !read(next.back_arm_rotation_center, offset("backArmRotationCenter"))
      || !read(next.front_hand_position, offset("frontHandPosition"))
      || !read(next.back_arm_offset, offset("backArmOffset"))
      || !read(next.vapor_trail_frames, config.get_int("vaporTrailFrames"))
      || !read(next.vapor_trail_cycle, config.get_float("vaporTrailCycle"))
      || !read(next.default_death_particles, config.get_string("deathParticles"))
      || !read(next.particle_emitters, config.get("particleEmitters"))
      || !read(next.default_movement_parameters, config.get("movementParameters"))
      || !read(next.player_movement_parameters, config.opt("playerMovementParameters")))
    return std::unexpected(std::move(*error));

  auto center = config.opt("headRotationCenter");
  if (!center)
    return std::unexpected(std::move(center.error()));
  if (*center) {
    auto parsed = pixel_vector(**center);
    if (!parsed)
      return std::unexpected(std::move(parsed.error()));
    next.head_rotation_center = *parsed;
  } else {
    next.head_rotation_center = Vec2F(0, -2) / tile_pixels;
  }

  bool movement_changed = next.default_movement_parameters != m_config.default_movement_parameters
      || next.player_movement_parameters != m_config.player_movement_parameters;
  m_config = std::move(next);
  return movement_changed;
}

std::expected<void, StarException> Humanoid::load_animation() {
  auto checked174 = m_base_config.opt("animation");
if (!checked174) return std::unexpected(std::move(checked174.error()));
auto animation_config = std::move(*checked174);
  m_use_animation = animation_config.has_value();

  auto checked175 = m_base_config.get_array("animationScripts", JsonArray());
if (!checked175) return std::unexpected(std::move(checked175.error()));
auto checked176 = json_to_string_list(std::move(*checked175));
if (!checked176) return std::unexpected(std::move(checked176.error()));
m_animation_scripts = std::move(*checked176);

  m_animation_states.clear();
  m_animation_states_backwards.clear();
  m_emote_animation_states.clear();
  m_portrait_animation_states.clear();

  String animation_path = ("/humanoid/" + m_identity.image_path.value_or(m_identity.species) + "/");
  if (m_use_animation)
    {
auto checked177 = NetworkedAnimator::create(m_root, *animation_config, animation_path);
if (!checked177) return std::unexpected(std::move(checked177.error()));
m_networked_animator = std::move(*checked177);
}
  else
    {
auto checked178 = NetworkedAnimator::create(m_root);
if (!checked178) return std::unexpected(std::move(checked178.error()));
m_networked_animator = std::move(*checked178);
}

  if (m_use_animation) {
    auto checked179 = m_base_config.get_string("frontHandItemPart", "frontHandItem");
if (!checked179) return std::unexpected(std::move(checked179.error()));
m_front_item_part = std::move(*checked179);
    auto checked180 = m_base_config.get_string("backHandItemPart", "backHandItem");
if (!checked180) return std::unexpected(std::move(checked180.error()));
m_back_item_part = std::move(*checked180);

    auto checked181 = m_base_config.get_string("headRotationPart", "head");
if (!checked181) return std::unexpected(std::move(checked181.error()));
auto checked182 = m_base_config.get_string("headRotationPartPoint", "rotationCenter");
if (!checked182) return std::unexpected(std::move(checked182.error()));
m_head_rotation_point = {std::move(*checked181), std::move(*checked182)};
    auto checked183 = m_base_config.get_string("frontArmRotationPart", "frontArm");
if (!checked183) return std::unexpected(std::move(checked183.error()));
auto checked184 = m_base_config.get_string("frontArmRotationPartPoint", "rotationCenter");
if (!checked184) return std::unexpected(std::move(checked184.error()));
m_front_arm_rotation_point = {std::move(*checked183), std::move(*checked184)};
    auto checked185 = m_base_config.get_string("backArmRotationPart", "backArm");
if (!checked185) return std::unexpected(std::move(checked185.error()));
auto checked186 = m_base_config.get_string("backArmRotationPartPoint", "rotationCenter");
if (!checked186) return std::unexpected(std::move(checked186.error()));
m_back_arm_rotation_point = {std::move(*checked185), std::move(*checked186)};

    auto checked187 = m_base_config.get_string("mouthOffsetPart", "head");
if (!checked187) return std::unexpected(std::move(checked187.error()));
auto checked188 = m_base_config.get_string("mouthOffsetPartPoint", "mouthOffset");
if (!checked188) return std::unexpected(std::move(checked188.error()));
m_mouth_offset_point = {std::move(*checked187), std::move(*checked188)};
    auto checked189 = m_base_config.get_string("headArmorOffsetPart", "headCosmetic");
if (!checked189) return std::unexpected(std::move(checked189.error()));
auto checked190 = m_base_config.get_string("headArmorOffsetPartPoint", "armorOffset");
if (!checked190) return std::unexpected(std::move(checked190.error()));
m_head_armor_offset_point = {std::move(*checked189), std::move(*checked190)};
    auto checked191 = m_base_config.get_string("chestArmorOffsetPart", "chestCosmetic");
if (!checked191) return std::unexpected(std::move(checked191.error()));
auto checked192 = m_base_config.get_string("chestArmorOffsetPartPoint", "armorOffset");
if (!checked192) return std::unexpected(std::move(checked192.error()));
m_chest_armor_offset_point = {std::move(*checked191), std::move(*checked192)};
    auto checked193 = m_base_config.get_string("legsArmorOffsetPart", "legsCosmetic");
if (!checked193) return std::unexpected(std::move(checked193.error()));
auto checked194 = m_base_config.get_string("legsArmorOffsetPartPoint", "armorOffset");
if (!checked194) return std::unexpected(std::move(checked194.error()));
m_legs_armor_offset_point = {std::move(*checked193), std::move(*checked194)};
    auto checked195 = m_base_config.get_string("backArmorOffsetPart", "backCosmetic");
if (!checked195) return std::unexpected(std::move(checked195.error()));
auto checked196 = m_base_config.get_string("backArmorOffsetPartPoint", "armorOffset");
if (!checked196) return std::unexpected(std::move(checked196.error()));
m_back_armor_offset_point = {std::move(*checked195), std::move(*checked196)};
    auto checked197 = m_base_config.get_string("feetOffsetPart", "body");
if (!checked197) return std::unexpected(std::move(checked197.error()));
auto checked198 = m_base_config.get_string("feetOffsetPartPoint", "feetOffset");
if (!checked198) return std::unexpected(std::move(checked198.error()));
m_feet_offset_point = {std::move(*checked197), std::move(*checked198)};
    auto checked199 = m_base_config.get_string("throwPart", "head");
if (!checked199) return std::unexpected(std::move(checked199.error()));
auto checked200 = m_base_config.get_string("throwPartPoint", "mouthOffset");
if (!checked200) return std::unexpected(std::move(checked200.error()));
m_throw_point = {std::move(*checked199), std::move(*checked200)};
    auto checked201 = m_base_config.get_string("interactPart", "body");
if (!checked201) return std::unexpected(std::move(checked201.error()));
auto checked202 = m_base_config.get_string("interactPartPoint", "interact");
if (!checked202) return std::unexpected(std::move(checked202.error()));
m_interact_point = {std::move(*checked201), std::move(*checked202)};

    auto checked203 = m_base_config.get_object("identityFramesetTags", JsonObject());
if (!checked203) return std::unexpected(std::move(checked203.error()));
auto checked204 = json_to_map_v<StringMap<String>>(std::move(*checked203), std::mem_fn(&Json::to_string));
if (!checked204) return std::unexpected(std::move(checked204.error()));
m_identity_frameset_tags = std::move(*checked204);

    {
auto checked205 = m_base_config.get_object("stateAnimations", JsonObject());
if (!checked205) return std::unexpected(std::move(checked205.error()));
for (auto const& pair : std::move(*checked205)) {
  HashMap<String, AnimationStateArgs> animations;
  {
    auto checked206 = pair.second.iterate_object();
    if (!checked206)
      return std::unexpected(std::move(checked206.error()));
    for (auto const& anim : std::move(*checked206)) {
      auto checked207 = anim.second.to_array();
      if (!checked207)
        return std::unexpected(std::move(checked207.error()));
      auto args = std::move(*checked207);
      auto checked208 = args.at(0);
      if (!checked208)
        return std::unexpected(std::move(checked208.error()));
      auto checked209 = checked208->get().to_string();
      if (!checked209)
        return std::unexpected(std::move(checked209.error()));
      auto checked210 = args.at(1);
      if (!checked210)
        return std::unexpected(std::move(checked210.error()));
      auto checked211 = checked210->get().to_bool();
      if (!checked211)
        return std::unexpected(std::move(checked211.error()));
      auto checked212 = args.at(2);
      if (!checked212)
        return std::unexpected(std::move(checked212.error()));
      auto checked213 = checked212->get().to_bool();
      if (!checked213)
        return std::unexpected(std::move(checked213.error()));
      animations.set(anim.first, {std::move(*checked209), std::move(*checked211), std::move(*checked213)});
    }
  }
  if (!state_names)
    return std::unexpected(state_names.error());
  auto checked214 = state_names->get_left(pair.first);
  if (!checked214)
    return std::unexpected(std::move(checked214.error()));
  m_animation_states.set(checked214->get(), animations);
}
}
    {
auto checked215 = m_base_config.get_object("stateAnimationsBackwards", JsonObject());
if (!checked215) return std::unexpected(std::move(checked215.error()));
for (auto const& pair : std::move(*checked215)) {
  HashMap<String, AnimationStateArgs> animations;
  {
    auto checked216 = pair.second.iterate_object();
    if (!checked216)
      return std::unexpected(std::move(checked216.error()));
    for (auto const& anim : std::move(*checked216)) {
      auto checked217 = anim.second.to_array();
      if (!checked217)
        return std::unexpected(std::move(checked217.error()));
      auto args = std::move(*checked217);
      auto checked218 = args.at(0);
      if (!checked218)
        return std::unexpected(std::move(checked218.error()));
      auto checked219 = checked218->get().to_string();
      if (!checked219)
        return std::unexpected(std::move(checked219.error()));
      auto checked220 = args.at(1);
      if (!checked220)
        return std::unexpected(std::move(checked220.error()));
      auto checked221 = checked220->get().to_bool();
      if (!checked221)
        return std::unexpected(std::move(checked221.error()));
      auto checked222 = args.at(2);
      if (!checked222)
        return std::unexpected(std::move(checked222.error()));
      auto checked223 = checked222->get().to_bool();
      if (!checked223)
        return std::unexpected(std::move(checked223.error()));
      animations.set(anim.first, {std::move(*checked219), std::move(*checked221), std::move(*checked223)});
    }
  }
  if (!state_names)
    return std::unexpected(state_names.error());
  auto checked224 = state_names->get_left(pair.first);
  if (!checked224)
    return std::unexpected(std::move(checked224.error()));
  m_animation_states_backwards.set(checked224->get(), animations);
}
}
    {
auto checked225 = m_base_config.get_object("emoteAnimations", JsonObject());
if (!checked225) return std::unexpected(std::move(checked225.error()));
for (auto const& pair : std::move(*checked225)) {
  HashMap<String, AnimationStateArgs> animations;
  {
    auto checked226 = pair.second.iterate_object();
    if (!checked226)
      return std::unexpected(std::move(checked226.error()));
    for (auto const& anim : std::move(*checked226)) {
      auto checked227 = anim.second.to_array();
      if (!checked227)
        return std::unexpected(std::move(checked227.error()));
      auto args = std::move(*checked227);
      auto checked228 = args.at(0);
      if (!checked228)
        return std::unexpected(std::move(checked228.error()));
      auto checked229 = checked228->get().to_string();
      if (!checked229)
        return std::unexpected(std::move(checked229.error()));
      auto checked230 = args.at(1);
      if (!checked230)
        return std::unexpected(std::move(checked230.error()));
      auto checked231 = checked230->get().to_bool();
      if (!checked231)
        return std::unexpected(std::move(checked231.error()));
      auto checked232 = args.at(2);
      if (!checked232)
        return std::unexpected(std::move(checked232.error()));
      auto checked233 = checked232->get().to_bool();
      if (!checked233)
        return std::unexpected(std::move(checked233.error()));
      animations.set(anim.first, {std::move(*checked229), std::move(*checked231), std::move(*checked233)});
    }
  }
  if (!emote_frame_names)
    return std::unexpected(emote_frame_names.error());
  auto emote = emote_frame_names->get_left(pair.first);
  if (!emote)
    return std::unexpected(std::move(emote.error()));
  m_emote_animation_states.set(emote->get(), std::move(animations));
}
}
    {
auto checked235 = m_base_config.get_object("portraitAnimations", JsonObject());
if (!checked235) return std::unexpected(std::move(checked235.error()));
for (auto const& pair : std::move(*checked235)) {
  HashMap<String, AnimationStateArgs> animations;
  {
    auto checked236 = pair.second.iterate_object();
    if (!checked236)
      return std::unexpected(std::move(checked236.error()));
    for (auto const& anim : std::move(*checked236)) {
      auto checked237 = anim.second.to_array();
      if (!checked237)
        return std::unexpected(std::move(checked237.error()));
      auto args = std::move(*checked237);
      auto checked238 = args.at(0);
      if (!checked238)
        return std::unexpected(std::move(checked238.error()));
      auto checked239 = checked238->get().to_string();
      if (!checked239)
        return std::unexpected(std::move(checked239.error()));
      auto checked240 = args.at(1);
      if (!checked240)
        return std::unexpected(std::move(checked240.error()));
      auto checked241 = checked240->get().to_bool();
      if (!checked241)
        return std::unexpected(std::move(checked241.error()));
      auto checked242 = args.at(2);
      if (!checked242)
        return std::unexpected(std::move(checked242.error()));
      auto checked243 = checked242->get().to_bool();
      if (!checked243)
        return std::unexpected(std::move(checked243.error()));
      animations.set(anim.first, {std::move(*checked239), std::move(*checked241), std::move(*checked243)});
    }
  }
  auto checked244 = enum_value<portrait_mode_names>(pair.first);
  if (!checked244)
    return std::unexpected(std::move(checked244.error()));
  m_portrait_animation_states.set(*checked244, animations);
}
}
    
}
  return {};
}

void Humanoid::wearable_removed(Wearable const& wearable) {
  auto& fashion = *m_fashion;

  auto set_tags = [&](HashMap<String, String> const& tags) {
    if (m_use_animation)
      for (auto const& tag : tags) {
        m_networked_animator->set_local_tag(tag.first);
      }
  };

  if (std::holds_alternative<WornHead>(wearable)) {
    auto const& head = std::get<WornHead>(wearable);
    fashion.worn_heads_changed = true;
    if (!head.mask_directives.empty())
      fashion.helmet_masks_changed = true;
    set_tags(head.animation_tags);
  } else if (std::holds_alternative<WornChest>(wearable)) {
    fashion.worn_chests_legs_changed = true;
    set_tags(std::get<WornChest>(wearable).animation_tags);
  } else if (std::holds_alternative<WornLegs>(wearable)) {
    fashion.worn_chests_legs_changed = true;
    set_tags(std::get<WornLegs>(wearable).animation_tags);
  } else if (std::holds_alternative<WornBack>(wearable)) {
    fashion.worn_backs_changed = true;
    set_tags(std::get<WornBack>(wearable).animation_tags);
  }
}

std::expected<void, StarException> Humanoid::remove_wearable(std::uint8_t slot) {
  auto& fashion = *m_fashion;
  auto checked245 = fashion.wearables.at(slot);
if (!checked245) return std::unexpected(std::move(checked245.error()));
Wearable& current = checked245->get();
  wearable_removed(current);
  current.emplace<0>();
  return {};
}

std::expected<void, StarException> Humanoid::set_wearable_from_head(std::uint8_t slot, HeadArmor const& head, Gender gender) {
  auto& fashion = *m_fashion;
  auto checked246 = fashion.wearables.at(slot);
if (!checked246) return std::unexpected(std::move(checked246.error()));
Wearable& current = checked246->get();
  if (std::holds_alternative<WornHead>(current))
    fashion.helmet_masks_changed |= std::get<WornHead>(current).mask_directives != head.maskDirectives();
  else {
    wearable_removed(current);
    fashion.worn_heads_changed = true;
    fashion.helmet_masks_changed |= !head.maskDirectives().empty();
  }
  auto& worn_head = current.emplace<WornHead>();
  worn_head.directives = head.directives(m_facing_direction == Direction::Left);
  worn_head.fullbright = head.fullbright();
  worn_head.frameset = head.frameset(gender);
  worn_head.mask_directives = head.maskDirectives();
  worn_head.animation_tags.clear();
  worn_head.animation_tags.set(Star::format("headCosmetic{}Frameset", slot+1), worn_head.frameset);
  worn_head.animation_tags.set(Star::format("headCosmetic{}Directives", slot+1), worn_head.directives.string());
  {
auto checked247 = head.instance_value("humanoidAnimationTags", JsonObject());
if (!checked247) return std::unexpected(std::move(checked247.error()));
auto checked248 = std::move(*checked247).iterate_object();
if (!checked248) return std::unexpected(std::move(checked248.error()));
for (auto const& tag : std::move(*checked248)) {
  auto checked249 = tag.first.replace("<slot>", Star::to_string(slot + 1));
  if (!checked249)
    return std::unexpected(std::move(checked249.error()));
  auto checked250 = m_networked_animator->apply_part_tags(m_head_armor_offset_point.first, std::move(*checked249));
  if (!checked250)
    return std::unexpected(std::move(checked250.error()));
  auto checked251 = tag.second.to_string();
  if (!checked251)
    return std::unexpected(std::move(checked251.error()));
  auto checked252 = m_networked_animator->apply_part_tags(m_head_armor_offset_point.first, std::move(*checked251));
  if (!checked252)
    return std::unexpected(std::move(checked252.error()));
  worn_head.animation_tags.set(std::move(*checked250), std::move(*checked252));
}
}
  return {};
}

std::expected<void, StarException> Humanoid::set_wearable_from_chest(std::uint8_t slot, ChestArmor const& chest, Gender gender) {
  auto& fashion = *m_fashion;
  auto checked253 = fashion.wearables.at(slot);
if (!checked253) return std::unexpected(std::move(checked253.error()));
Wearable& current = checked253->get();
  if (!std::holds_alternative<WornChest>(current) && !std::holds_alternative<WornLegs>(current)) {
    wearable_removed(current);
    fashion.worn_chests_legs_changed = true;
  }

  auto& worn_chest = current.emplace<WornChest>();
  worn_chest.directives = chest.directives(m_facing_direction == Direction::Left);
  worn_chest.fullbright = chest.fullbright();
  worn_chest.frameset = chest.bodyFrameset(gender);
  worn_chest.back_sleeve_frameset = chest.backSleeveFrameset(gender);
  worn_chest.front_sleeve_frameset = chest.frontSleeveFrameset(gender);
  worn_chest.animation_tags.clear();
  worn_chest.animation_tags.set(Star::format("chestCosmetic{}Frameset", slot+1), worn_chest.frameset);
  worn_chest.animation_tags.set(Star::format("frontSleeve{}Frameset", slot+1), worn_chest.front_sleeve_frameset);
  worn_chest.animation_tags.set(Star::format("backSleeve{}Frameset", slot+1), worn_chest.back_sleeve_frameset);
  worn_chest.animation_tags.set(Star::format("chestCosmetic{}Directives", slot+1), worn_chest.directives.string());
  {
auto checked254 = chest.instance_value("humanoidAnimationTags", JsonObject());
if (!checked254) return std::unexpected(std::move(checked254.error()));
auto checked255 = std::move(*checked254).iterate_object();
if (!checked255) return std::unexpected(std::move(checked255.error()));
for (auto const& tag : std::move(*checked255)) {
  auto checked256 = tag.first.replace("<slot>", Star::to_string(slot + 1));
  if (!checked256)
    return std::unexpected(std::move(checked256.error()));
  auto checked257 = m_networked_animator->apply_part_tags(m_chest_armor_offset_point.first, std::move(*checked256));
  if (!checked257)
    return std::unexpected(std::move(checked257.error()));
  auto checked258 = tag.second.to_string();
  if (!checked258)
    return std::unexpected(std::move(checked258.error()));
  auto checked259 = m_networked_animator->apply_part_tags(m_chest_armor_offset_point.first, std::move(*checked258));
  if (!checked259)
    return std::unexpected(std::move(checked259.error()));
  worn_chest.animation_tags.set(std::move(*checked257), std::move(*checked259));
}
}
  return {};
}

std::expected<void, StarException> Humanoid::set_wearable_from_legs(std::uint8_t slot, LegsArmor const& legs, Gender gender) {
  auto& fashion = *m_fashion;
  auto checked260 = fashion.wearables.at(slot);
if (!checked260) return std::unexpected(std::move(checked260.error()));
Wearable& current = checked260->get();
  if (!std::holds_alternative<WornChest>(current) && !std::holds_alternative<WornLegs>(current)) {
    wearable_removed(current);
    fashion.worn_chests_legs_changed = true;
  }

  auto& worn_legs = current.emplace<WornLegs>();
  worn_legs.directives = legs.directives(m_facing_direction == Direction::Left);
  worn_legs.fullbright = legs.fullbright();
  worn_legs.frameset = legs.frameset(gender);
  worn_legs.animation_tags.clear();
  worn_legs.animation_tags.set(Star::format("legsCosmetic{}Frameset", slot+1), worn_legs.frameset);
  worn_legs.animation_tags.set(Star::format("legsCosmetic{}Directives", slot+1), worn_legs.directives.string());
  {
auto checked261 = legs.instance_value("humanoidAnimationTags", JsonObject());
if (!checked261) return std::unexpected(std::move(checked261.error()));
auto checked262 = std::move(*checked261).iterate_object();
if (!checked262) return std::unexpected(std::move(checked262.error()));
for (auto const& tag : std::move(*checked262)) {
  auto checked263 = tag.first.replace("<slot>", Star::to_string(slot + 1));
  if (!checked263)
    return std::unexpected(std::move(checked263.error()));
  auto checked264 = m_networked_animator->apply_part_tags(m_legs_armor_offset_point.first, std::move(*checked263));
  if (!checked264)
    return std::unexpected(std::move(checked264.error()));
  auto checked265 = tag.second.to_string();
  if (!checked265)
    return std::unexpected(std::move(checked265.error()));
  auto checked266 = m_networked_animator->apply_part_tags(m_legs_armor_offset_point.first, std::move(*checked265));
  if (!checked266)
    return std::unexpected(std::move(checked266.error()));
  worn_legs.animation_tags.set(std::move(*checked264), std::move(*checked266));
}
}
  return {};
}

std::expected<void, StarException> Humanoid::set_wearable_from_back(std::uint8_t slot, BackArmor const& back, Gender gender) {
  auto& fashion = *m_fashion;
  auto checked267 = fashion.wearables.at(slot);
if (!checked267) return std::unexpected(std::move(checked267.error()));
Wearable& current = checked267->get();
  if (!std::holds_alternative<WornBack>(current)) {
    wearable_removed(current);
    fashion.worn_backs_changed = true;
  }

  auto& worn_back = current.emplace<WornBack>();
  worn_back.directives = back.directives(m_facing_direction == Direction::Left);
  worn_back.fullbright = back.fullbright();
  worn_back.frameset = back.frameset(gender);
  auto checked268 = back.instance_value("rotateWithHead", false);
if (!checked268) return std::unexpected(std::move(checked268.error()));
auto checked269 = std::move(*checked268).opt_bool();
if (!checked269) return std::unexpected(std::move(checked269.error()));
worn_back.rotate_with_head = std::move(*checked269).value_or(false);
  worn_back.animation_tags.clear();
  worn_back.animation_tags.set(Star::format("backCosmetic{}Frameset", slot+1), worn_back.frameset);
  worn_back.animation_tags.set(Star::format("backCosmetic{}Directives", slot+1), worn_back.directives.string());
  {
auto checked270 = back.instance_value("humanoidAnimationTags", JsonObject());
if (!checked270) return std::unexpected(std::move(checked270.error()));
auto checked271 = std::move(*checked270).iterate_object();
if (!checked271) return std::unexpected(std::move(checked271.error()));
for (auto const& tag : std::move(*checked271)) {
  auto checked272 = tag.first.replace("<slot>", Star::to_string(slot + 1));
  if (!checked272)
    return std::unexpected(std::move(checked272.error()));
  auto checked273 = m_networked_animator->apply_part_tags(m_back_armor_offset_point.first, std::move(*checked272));
  if (!checked273)
    return std::unexpected(std::move(checked273.error()));
  auto checked274 = tag.second.to_string();
  if (!checked274)
    return std::unexpected(std::move(checked274.error()));
  auto checked275 = m_networked_animator->apply_part_tags(m_back_armor_offset_point.first, std::move(*checked274));
  if (!checked275)
    return std::unexpected(std::move(checked275.error()));
  worn_back.animation_tags.set(std::move(*checked273), std::move(*checked275));
}
}
  return {};
}

static constexpr std::array<std::uint8_t, 21> chest_legs_sort_order{255, 14, 8, 2, 0, 15, 9, 3, 1, 4, 5, 6, 7, 10, 11, 12, 13, 16, 17, 18, 19};

template <humanoid_pose_animator Animator>
void Humanoid::refresh_wearables(Fashion& fashion, Animator& animator) const {
  bool worn_heads_changed = fashion.worn_heads_changed;
  bool worn_chests_legs_changed = fashion.worn_chests_legs_changed;
  bool worn_backs_changed = fashion.worn_backs_changed;
  bool helmet_masks_changed = fashion.helmet_masks_changed;
  if (!worn_heads_changed && !worn_chests_legs_changed && !worn_backs_changed && !helmet_masks_changed)
    return;

  if (worn_heads_changed)
    fashion.worn_heads.fill(0);
  if (worn_chests_legs_changed)
    fashion.worn_chests_legs.fill(0);
  if (worn_backs_changed)
    fashion.worn_backs.fill(0);
  if (helmet_masks_changed)
    fashion.helmet_mask_directives_group.clear();

  auto set_tags = [&](HashMap<String, String> const& tags) {
    if (m_use_animation)
      for (auto const& [name, value] : tags)
        animator.set_local_tag(name, value);
  };

  std::uint8_t head_index = 0, chests_legs_index = 0, backs_index = 0;
  for (std::uint8_t i = 0; i != fashion.wearables.size(); ++i) {
    auto& wearable = fashion.wearables[i];
    if (std::holds_alternative<std::monostate>(wearable))
      continue;
    else if (std::holds_alternative<WornHead>(wearable)) {
      auto const& head = std::get<WornHead>(wearable);
      if (helmet_masks_changed)
        fashion.helmet_mask_directives_group += head.mask_directives;
      if (worn_heads_changed) {
        fashion.worn_heads[head_index++] = i + 1;
        set_tags(head.animation_tags);
      }
    } else if (std::holds_alternative<WornChest>(wearable) || std::holds_alternative<WornLegs>(wearable)) {
      if (worn_chests_legs_changed) {
        fashion.worn_chests_legs[chests_legs_index++] = i + 1;
        if (std::holds_alternative<WornChest>(wearable))
          set_tags(std::get<WornChest>(wearable).animation_tags);
        else
          set_tags(std::get<WornLegs>(wearable).animation_tags);
      }

    } else if (std::holds_alternative<WornBack>(wearable)) {
      if (worn_backs_changed) {
        fashion.worn_backs[backs_index++] = i + 1;
        set_tags(std::get<WornBack>(wearable).animation_tags);
      }
    }
  }
  if (worn_chests_legs_changed) {
    std::sort(fashion.worn_chests_legs.begin(), fashion.worn_chests_legs.end(), [](std::uint8_t a, std::uint8_t b) {
      return chest_legs_sort_order[a] < chest_legs_sort_order[b];
    });
  }

  if (m_use_animation)
    animator.set_local_tag("helmetMaskDirectives", fashion.helmet_mask_directives_group.to_string());

  fashion.worn_heads_changed = fashion.worn_chests_legs_changed = fashion.worn_backs_changed = fashion.helmet_masks_changed = false;
}

const Directives null_directives = {};
const String null_frameset = "";

Directives const& Humanoid::head_armor_directives() const {
  if (auto head = get_last_wearable_of_type<WornHead>())
    return head->get().directives;
  return null_directives;
};

String const& Humanoid::head_armor_frameset() const {
  if (auto head = get_last_wearable_of_type<WornHead>())
    return head->get().frameset;
  return null_frameset;
};

Directives const& Humanoid::chest_armor_directives() const {
  if (auto chest = get_last_wearable_of_type<WornChest>())
    return chest->get().directives;
  return null_directives;
};

String const& Humanoid::chest_armor_frameset() const {
  if (auto chest = get_last_wearable_of_type<WornChest>())
    return chest->get().frameset;
  return null_frameset;
};

String const& Humanoid::back_sleeve_frameset() const {
  if (auto chest = get_last_wearable_of_type<WornChest>())
    return chest->get().back_sleeve_frameset;
  return null_frameset;
};

String const& Humanoid::front_sleeve_frameset() const {
  if (auto chest = get_last_wearable_of_type<WornChest>())
    return chest->get().front_sleeve_frameset;
  return null_frameset;
};

Directives const& Humanoid::legs_armor_directives() const {
  if (auto legs = get_last_wearable_of_type<WornLegs>())
    return legs->get().directives;
  return null_directives;
};

String const& Humanoid::legs_armor_frameset() const {
  if (auto legs = get_last_wearable_of_type<WornLegs>())
    return legs->get().frameset;
  return null_frameset;
};

Directives const& Humanoid::back_armor_directives() const {
  if (auto back = get_last_wearable_of_type<WornBack>())
    return back->get().directives;
  return null_directives;
};

String const& Humanoid::back_armor_frameset() const {
  if (auto back = get_last_wearable_of_type<WornBack>())
    return back->get().frameset;
  return null_frameset;
};


std::expected<void, StarException> Humanoid::set_state(State state) {
  if (m_state == state)
    return {};
  if (static_cast<unsigned>(state) >= state_size)
    return std::unexpected(StarException("Invalid humanoid movement state"));
  if (m_use_animation) {
    auto applied = apply_animation_group(*m_networked_animator, movement_animation_group(state, m_moving_backwards), false);
    if (!applied) return applied;
  }
  m_state = state;
  m_previous_animation_timer = m_animation_timer = 0;
  return {};
}

template <humanoid_pose_animator Animator>
std::expected<void, StarException> Humanoid::apply_animation_group(Animator& animator, AnimationGroupRef group, bool restart) {
  if (!group)
    return {};
  auto const& states = group->get();
  if constexpr (std::same_as<Animator, NetworkedAnimator>) {
    // Both lookups are against immutable definitions. There are no other
    // fallible operations in a logical set_local_state once its target exists.
    if (states.size() > 1) {
      for (auto const& [type, args] : states)
        if (auto valid = animator.validate_local_state(type, args.state); !valid)
          return valid;
    }
  }
  for (auto const& [type, args] : states) {
    auto changed = animator.set_local_state(type, args.state, restart || args.start_new, args.reverse);
    if (!changed) return std::unexpected(std::move(changed.error()));
  }
  return {};
}

std::expected<void, StarException> Humanoid::set_emote_state(HumanoidEmote state) {
  if (static_cast<std::size_t>(state) >= emote_size)
    return std::unexpected(StarException("Invalid humanoid emote"));
  if (m_emote_state == state)
    return {};
  if (m_use_animation) {
    auto applied = apply_animation_group(*m_networked_animator, std::as_const(m_emote_animation_states).find_ref(state), false);
    if (!applied) return applied;
  }
  m_emote_state = state;
  m_emote_animation_timer = m_previous_emote_animation_timer = 0;
  return {};
}

std::expected<void, StarException> Humanoid::set_dance(std::optional<String> const& name) {
  if (name == m_dance)
    return {};

  CachedDance next;
  if (name) {
    auto resolved = load_dance(*name);
    if (!resolved)
      return std::unexpected(std::move(resolved.error()));
    next = std::move(*resolved);
    if (m_use_animation) {
      auto has_state = m_networked_animator->has_state("dance", name);
      if (!has_state)
        return std::unexpected(std::move(has_state.error()));
      if (*has_state) {
        auto changed = m_networked_animator->set_local_state("dance", *name);
        if (!changed)
          return std::unexpected(std::move(changed.error()));
      }
    }
  }

  m_dance = name;
  m_cached_dance = std::move(next);
  m_dance_timer = m_previous_dance_timer = 0;
  return {};
}

void Humanoid::set_facing_direction(Direction facing_direction) {
  m_facing_direction = facing_direction;
  if (m_use_animation)
    m_networked_animator->set_flipped(m_facing_direction == Direction::Left);
}

std::expected<void, StarException> Humanoid::set_moving_backwards(bool backwards) {
  if (m_moving_backwards == backwards)
    return {};
  if (m_use_animation) {
    auto applied = apply_animation_group(*m_networked_animator, movement_animation_group(m_state, backwards), false);
    if (!applied) return applied;
  }
  m_moving_backwards = backwards;
  return {};
}

void Humanoid::set_head_rotation(float head_rotation) {
  m_head_rotation_target = head_rotation;
}

void Humanoid::set_rotation(float rotation) {
  m_rotation = rotation;
}

void Humanoid::set_scale(Vec2F scale) {
  m_scale = scale;
}

void Humanoid::set_vapor_trail(bool enabled) {
  m_draw_vapor_trail = enabled;
}

Humanoid::State Humanoid::state() const {
  return m_state;
}

HumanoidEmote Humanoid::emote_state() const {
  return m_emote_state;
}

std::optional<String> const& Humanoid::dance() const {
  return m_dance;
}

std::expected<bool, StarException> Humanoid::dance_cyclic_or_ended() const {
  auto resolved = current_dance();
  if (!resolved)
    return std::unexpected(std::move(resolved.error()));
  if (!*resolved)
    return false;
  auto const& dance = (*resolved)->get();
  return dance.cyclic || m_dance_timer > dance.duration;
}

Direction Humanoid::facing_direction() const {
  return m_facing_direction;
}

bool Humanoid::moving_backwards() const {
  return m_moving_backwards;
}

void Humanoid::set_hand_parameters(ToolHand hand, bool holding_item, float angle, float item_angle, bool two_handed,
    bool recoil, bool outside_of_hand) {
  auto& hand_info = const_cast<HandDrawingInfo&>(get_hand(hand));
  hand_info.holding_item = holding_item;
  hand_info.angle = angle;
  hand_info.item_angle = item_angle;
  hand_info.recoil = recoil;
  hand_info.outside_of_hand = outside_of_hand;
  if (hand == ToolHand::Primary)
    m_two_handed = two_handed;
}

void Humanoid::set_hand_frame_overrides(ToolHand hand, StringView back, StringView front) {
  auto& hand_info = const_cast<HandDrawingInfo&>(get_hand(hand));
  // some users stick directives in these?? better make sure they don't break with custom clothing
  std::size_t  back_end =  back.utf8().find('?');
  std::size_t front_end = front.utf8().find('?');
  Directives  back_directives =  back_end == npos ? Directives() : Directives( back.utf8().substr( back_end));
  Directives front_directives = front_end == npos ? Directives() : Directives(front.utf8().substr(front_end));
  if ( back_end != npos)  back =  back.utf8().substr(0,  back_end);
  if (front_end != npos) front = front.utf8().substr(0, front_end);
  hand_info. back_frame = ! back.empty() ? std::move( back) : "rotation";
  hand_info.front_frame = !front.empty() ? std::move(front) : "rotation";
  hand_info. back_directives = ! back_directives.empty() ? std::move( back_directives) : Directives();
  hand_info.front_directives = !front_directives.empty() ? std::move(front_directives) : Directives();
}

void Humanoid::set_hand_drawables(ToolHand hand, List<Drawable> drawables) {
  const_cast<HandDrawingInfo&>(get_hand(hand)).item_drawables = std::move(drawables);
}

void Humanoid::set_hand_non_rotated_drawables(ToolHand hand, List<Drawable> drawables) {
  const_cast<HandDrawingInfo&>(get_hand(hand)).non_rotated_drawables = std::move(drawables);
}

bool Humanoid::hand_holding_item(ToolHand hand) const {
  return get_hand(hand).holding_item;
}

std::expected<void, StarException> Humanoid::animate(float dt, std::optional<std::reference_wrapper<NetworkedAnimator::DynamicTarget>> dynamic_target) {
  m_previous_animation_timer = m_animation_timer;
  m_previous_emote_animation_timer = m_emote_animation_timer;
  m_previous_dance_timer = m_dance_timer;
  m_previous_pose = m_current_pose;
  m_animation_timer += dt;
  m_emote_animation_timer += dt;
  m_dance_timer += dt;
  auto head_rotation_enabled = m_root.humanoidHeadRotation();
  if (!head_rotation_enabled) return std::unexpected(std::move(head_rotation_enabled.error()));
  float head_rotation_target = head_rotation_enabled->get() ? m_head_rotation_target : 0.f;
  m_head_rotation = (head_rotation_target - (head_rotation_target - m_head_rotation) * std::powf(.333333f, dt * 60.f));
  m_current_pose = PresentationPose{m_head_rotation, m_rotation, m_scale,
      m_primary_hand.angle, m_primary_hand.item_angle, m_alt_hand.angle, m_alt_hand.item_angle,
      m_primary_hand.holding_item, m_alt_hand.holding_item, m_facing_direction, m_two_handed};
  if (!m_presentation_history || m_previous_pose.facing != m_current_pose.facing || m_previous_pose.two_handed != m_current_pose.two_handed)
    m_previous_pose = m_current_pose;
  m_presentation_history = true;
  auto posed = prepare_animator_pose();
  if (!posed) return std::unexpected(std::move(posed.error()));

  auto checked285 = m_networked_animator->update(dt, dynamic_target);
if (!checked285) return std::unexpected(std::move(checked285.error()));
  return {};
}

std::expected<void, StarException> Humanoid::reset_animation() {
  auto head_rotation = m_root.humanoidHeadRotation();
  if (!head_rotation) return std::unexpected(std::move(head_rotation.error()));
  auto head = head_rotation->get() ? 0.f : m_head_rotation_target;
  if (m_use_animation) {
    refresh_wearables(*m_fashion, *m_networked_animator);
    auto step = dance_step_at(0.f);
    if (!step) return std::unexpected(std::move(step.error()));
    auto reset = m_networked_animator->reset_local_animation([&](NetworkedAnimator::Sample& staged) -> std::expected<void, StarException> {
      auto emote = apply_animation_group(staged, std::as_const(m_emote_animation_states).find_ref(m_emote_state), true);
      if (!emote) return emote;
      auto movement = apply_animation_group(staged, movement_animation_group(m_state, m_moving_backwards), true);
      if (!movement) return movement;
      auto const& front = m_facing_direction == Direction::Left || m_two_handed ? m_primary_hand : m_alt_hand;
      auto const& back = m_facing_direction == Direction::Right || m_two_handed ? m_primary_hand : m_alt_hand;
      return apply_animator_pose(staged, front, back, front.angle, back.angle, head, *step, true);
    });
    if (!reset) return reset;
  }
  m_head_rotation = head;
  m_animation_timer = m_emote_animation_timer = m_dance_timer = 0;
  m_previous_animation_timer = m_previous_emote_animation_timer = m_previous_dance_timer = 0;
  m_presentation_history = false;
  return {};
}

Vec2F Humanoid::base_head_offset() const {
  switch (m_state) {
    case Idle: return m_identity.personality.head_offset / tile_pixels;
    case Run: return m_config.head_run_offset;
    case Swim:
    case SwimIdle: return m_config.head_swim_offset;
    case Duck: return m_config.head_duck_offset;
    case Sit: return m_config.head_sit_offset;
    case Lay: return m_config.head_lay_offset;
    default: return {};
  }
}

template <humanoid_pose_animator Animator>
std::expected<void, StarException> Humanoid::apply_animator_pose(Animator& animator, HandDrawingInfo const& front_hand,
    HandDrawingInfo const& back_hand, float front_angle, float back_angle, float head_rotation,
    std::optional<std::reference_wrapper<DanceStep const>> dance_step, bool with_items) {
  auto const& fashion = *m_fashion;
  if (auto reset = animator.reset_local_transformation_group("headRotation"); !reset)
    return reset;
  if (auto reset = animator.reset_local_transformation_group("bodyHeadRotation"); !reset)
    return reset;
  for (auto i : fashion.worn_backs) {
    if (i == 0)
      break;
    if (auto reset = animator.reset_local_transformation_group("backCosmetic" + Star::to_string(i) + "Rotation"); !reset)
      return reset;
  }
  if (head_rotation != 0.f) {
    float dir = numerical_direction(m_facing_direction);
    float head_x = head_rotation / (std::numbers::pi_v<float> * 2.f);
    Vec2F translate = {-(state() == State::Run ? (std::fmaxf(head_x, 0.f) * 2.f) : head_x),
                       -std::fabsf(head_rotation / (std::numbers::pi_v<float> * 4.f))};
    auto point = animator.part_property(m_head_rotation_point.first, m_head_rotation_point.second);
    if (!point) return std::unexpected(std::move(point.error()));
    auto center = json_to_vec2f(*point);
    if (!center) return std::unexpected(std::move(center.error()));
    auto transform = animator.part_transformation(m_head_rotation_point.first);
    if (!transform) return std::unexpected(std::move(transform.error()));
    auto body_center = transform->transform_point(*center);
    auto rotate_head = [&](String const& group, Vec2F const& pivot) -> std::expected<void, StarException> {
      if (auto rotated = animator.rotate_local_transformation_group(group, head_rotation * dir, pivot); !rotated)
        return rotated;
      return animator.translate_local_transformation_group(group, translate);
    };
    if (auto rotated = rotate_head("headRotation", *center); !rotated) return rotated;
    if (auto rotated = rotate_head("bodyHeadRotation", *center); !rotated) return rotated;
    for (auto i : fashion.worn_backs) {
      if (i == 0)
        break;
      if (std::get<WornBack>(fashion.wearables[std::size_t(i) - 1]).rotate_with_head) {
        if (auto rotated = rotate_head("backCosmetic" + Star::to_string(i) + "Rotation", body_center); !rotated)
          return rotated;
      }
    }
  }
  if (auto reset = animator.reset_local_transformation_group("backArmRotation"); !reset) return reset;
  if (auto reset = animator.reset_local_transformation_group("frontArmRotation"); !reset) return reset;

  auto dance_frame = [&](String const& state, String&& tag, std::optional<String> const& frame)
      -> std::expected<void, StarException> {
    if (frame) {
      auto exact = animator.has_state(state, frame);
      if (!exact) return std::unexpected(std::move(exact.error()));
      animator.set_local_tag(std::move(tag), frame);
      auto changed = *exact ? animator.set_local_state(state, *frame) : animator.set_local_state(state, "dance");
      if (!changed) return std::unexpected(std::move(changed.error()));
    } else {
      auto idle = animator.set_local_state(state, "idle");
      if (!idle) return std::unexpected(std::move(idle.error()));
      animator.set_local_tag(std::move(tag));
    }
    return {};
  };
  if (dance_step) {
    auto const& step = dance_step->get();
    // Authored movement/personality groups already supply the ordinary head
    // offset. A step replaces just that translation, leaving bob and aim intact.
    auto delta = step.head_offset / tile_pixels - base_head_offset();
    if (delta != Vec2F{}) {
      if (auto moved = animator.translate_local_transformation_group("headRotation", delta); !moved) return moved;
    }
    if (auto frame = dance_frame("bodyDance", "bodyDanceFrame", step.body_frame); !frame) return frame;
    auto dance_arm = [&](String const& rotation_group, Vec2F const& offset, float angle, String const& dance_state,
                         String&& tag, std::optional<String> const& frame, String const& arm_state)
        -> std::expected<void, StarException> {
      if (auto moved = animator.translate_local_transformation_group(rotation_group, offset / tile_pixels); !moved) return moved;
      if (auto rotated = animator.rotate_local_transformation_group(rotation_group, angle); !rotated) return rotated;
      if (auto applied = dance_frame(dance_state, std::move(tag), frame); !applied) return applied;
      auto idle = animator.set_local_state(arm_state, "idle");
      if (!idle) return std::unexpected(std::move(idle.error()));
      return {};
    };
    if (auto back = dance_arm("backArmRotation", step.back_arm_offset, step.back_arm_rotation, "backArmDance",
                              "backArmDanceFrame", step.back_arm_frame, "backArm"); !back) return back;
    return dance_arm("frontArmRotation", step.front_arm_offset, step.front_arm_rotation, "frontArmDance",
                     "frontArmDanceFrame", step.front_arm_frame, "frontArm");
  }

  if (auto body = dance_frame("bodyDance", "bodyDanceFrame", {}); !body) return body;
  auto aim_arm = [&](bool front, HandDrawingInfo const& hand, float angle) -> std::expected<void, StarException> {
    if (auto clear = dance_frame(front ? String("frontArmDance") : String("backArmDance"),
                                 front ? String("frontArmDanceFrame") : String("backArmDanceFrame"), {}); !clear)
      return clear;
    auto const& pivot = front ? m_front_arm_rotation_point : m_back_arm_rotation_point;
    auto property = animator.part_property(pivot.first, pivot.second);
    if (!property) return std::unexpected(std::move(property.error()));
    auto center = json_to_vec2f(*property);
    if (!center) return std::unexpected(std::move(center.error()));
    auto group = front ? String("frontArmRotation") : String("backArmRotation");
    if (auto rotated = animator.rotate_local_transformation_group(group, angle, *center); !rotated) return rotated;
    if (hand.recoil) {
      if (auto moved = animator.translate_local_transformation_group(group, m_config.recoil_offset); !moved) return moved;
    }
    auto state = front ? String("frontArm") : String("backArm");
    if (hand.holding_item && with_items) {
      auto const& frame = front ? hand.front_frame : hand.back_frame;
      animator.set_local_tag(front ? String("frontArmFrame") : String("backArmFrame"), frame);
      auto exact = animator.has_state(state, frame);
      if (!exact) return std::unexpected(std::move(exact.error()));
      auto arm = *exact ? animator.set_local_state(state, frame) : animator.set_local_state(state, "rotation");
      if (!arm) return std::unexpected(std::move(arm.error()));
      auto item = animator.set_local_state(front ? String("frontHandItem") : String("backHandItem"),
                                         hand.outside_of_hand ? String("outside") : String("inside"));
      if (!item) return std::unexpected(std::move(item.error()));
    } else {
      auto idle = animator.set_local_state(state, "idle");
      if (!idle) return std::unexpected(std::move(idle.error()));
    }
    return {};
  };
  if (auto back = aim_arm(false, back_hand, back_angle); !back) return back;
  return aim_arm(true, front_hand, front_angle);
}

std::expected<void, StarException> Humanoid::prepare_animator_pose() {
  return prepare_animator_pose(*m_networked_animator, 1.f, true);
}

std::expected<void, StarException> Humanoid::prepare_animator_pose(NetworkedAnimator::Sample& sample, bool with_items) {
  return prepare_animator_pose(sample, sample.interpolation(), with_items);
}

template <humanoid_pose_animator Animator>
std::expected<void, StarException> Humanoid::prepare_animator_pose(Animator& animator, float interpolation, bool with_items) {
  if (!m_use_animation)
    return {};
  interpolation = std::clamp(interpolation, 0.f, 1.f);
  refresh_wearables(*m_fashion, *m_networked_animator);
  auto dance_step = sample_dance_step(interpolation);
  if (!dance_step)
    return std::unexpected(std::move(dance_step.error()));
  bool front_primary = m_facing_direction == Direction::Left || m_two_handed;
  bool back_primary = m_facing_direction == Direction::Right || m_two_handed;
  auto const& front = front_primary ? m_primary_hand : m_alt_hand;
  auto const& back = back_primary ? m_primary_hand : m_alt_hand;
  auto angle = [&](HandDrawingInfo const& hand, bool primary) {
    if (!m_presentation_history || m_previous_pose.facing != m_facing_direction || m_previous_pose.two_handed != m_two_handed
        || hand.holding_item != (primary ? m_previous_pose.primary_holding : m_previous_pose.alt_holding))
      return hand.angle;
    return angle_lerp(interpolation, primary ? m_previous_pose.primary_angle : m_previous_pose.alt_angle, hand.angle);
  };
  float head =
    m_presentation_history ? angle_lerp(interpolation, m_previous_pose.head_rotation, m_head_rotation) : m_head_rotation;
  return apply_animator_pose(animator, front, back, angle(front, front_primary), angle(back, back_primary), head, *dance_step, with_items);
}

namespace {
// Keep the database alive for this draw only. Item-only and empty draws never acquire it.
struct HumanoidImageLayers {
  Root& root;
  float pixel_size;
  std::shared_ptr<ImageMetadataDatabase const> metadata = {};

  std::expected<Drawable, StarException> make(std::string_view image, Vec2F const& position,
      std::initializer_list<std::reference_wrapper<Directives const>> directives = {},
      std::optional<std::reference_wrapper<DirectivesGroup const>> mask = {}) {
    if (!metadata) {
      auto acquired = root.image_metadata_database();
      if (!acquired) return std::unexpected(std::move(acquired.error()));
      metadata = std::move(*acquired);
    }
    auto layer = Drawable::make_image(*metadata, AssetPath(image), pixel_size, true, position);
    if (!layer) return layer;
    auto& part = layer->image_part();
    for (auto entry : directives) {
      auto applied = part.add_directives(*metadata, entry.get(), true);
      if (!applied) return std::unexpected(std::move(applied.error()));
    }
    if (mask) {
      auto applied = part.add_directives_group(*metadata, mask->get(), true);
      if (!applied) return std::unexpected(std::move(applied.error()));
    }
    return layer;
  }
};
}

std::expected<List<Drawable>, StarException> Humanoid::render(bool with_items, bool with_rotation_and_scale, float interpolation) {
  auto sample = m_networked_animator->sample(interpolation);
  return render(sample, with_items, with_rotation_and_scale);
}

std::expected<List<Drawable>, StarException> Humanoid::render(NetworkedAnimator::Sample& sample, bool with_items, bool with_rotation_and_scale) {
  float interpolation = sample.interpolation();
  float head_rotation = m_presentation_history ? angle_lerp(interpolation, m_previous_pose.head_rotation, m_head_rotation) : m_head_rotation;
  float rotation = m_presentation_history ? angle_lerp(interpolation, m_previous_pose.rotation, m_rotation) : m_rotation;
  Vec2F scale = m_presentation_history ? lerp(interpolation, m_previous_pose.scale, m_scale) : m_scale;
  auto& fashion = *m_fashion;
  refresh_wearables(fashion, *m_networked_animator);
  auto sampled = sample_dance_step(interpolation);
  if (!sampled) return std::unexpected(std::move(sampled.error()));
  auto const& dance_step = *sampled;

  bool front_primary = m_facing_direction == Direction::Left || m_two_handed;
  bool back_primary = m_facing_direction == Direction::Right || m_two_handed;
  auto const& front_hand = front_primary ? m_primary_hand : m_alt_hand;
  auto const& back_hand = back_primary ? m_primary_hand : m_alt_hand;
  auto sample_angle = [&](HandDrawingInfo const& hand, bool primary, bool item) {
    float angle = item ? hand.item_angle : hand.angle;
    if (!m_presentation_history || m_previous_pose.facing != m_facing_direction || m_previous_pose.two_handed != m_two_handed
        || hand.holding_item != (primary ? m_previous_pose.primary_holding : m_previous_pose.alt_holding))
      return angle;
    float previous = item ? (primary ? m_previous_pose.primary_item_angle : m_previous_pose.alt_item_angle)
                          : (primary ? m_previous_pose.primary_angle : m_previous_pose.alt_angle);
    return angle_lerp(interpolation, previous, angle);
  };
  float front_angle = sample_angle(front_hand, front_primary, false);
  float back_angle = sample_angle(back_hand, back_primary, false);
  auto append_prefix = [&](List<Drawable>& output) {
    if (with_items) {
      output.append_range(m_alt_hand.non_rotated_drawables);
      output.append_range(m_primary_hand.non_rotated_drawables);
    }
  };

  if (m_use_animation) {
    auto posed = apply_animator_pose(sample, front_hand, back_hand, front_angle, back_angle, head_rotation, dance_step, with_items);
    if (!posed) return std::unexpected(std::move(posed.error()));
    auto item_drawables = [&](HandDrawingInfo const& hand, bool visible) -> std::optional<std::reference_wrapper<List<Drawable> const>> {
      if (!dance_step && with_items && hand.holding_item && visible)
        return std::cref(hand.item_drawables);
      return std::nullopt;
    };
    sample.set_part_drawables(m_back_item_part, item_drawables(back_hand, !m_two_handed));
    sample.set_part_drawables(m_front_item_part, item_drawables(front_hand, true));
    auto layers = sample.drawables();
    if (!layers) return layers;
    if (with_rotation_and_scale) {
      Drawable::rotate_all(*layers, rotation);
      Drawable::scale_all(*layers, scale);
    }
    if (!with_items || (m_alt_hand.non_rotated_drawables.empty() && m_primary_hand.non_rotated_drawables.empty())) {
      Drawable::rebase_all(*layers);
      return layers;
    }
    // Non-rotated animator attachments bypass both transforms, but are rebased with the body.
    List<Drawable> result;
    result.reserve(m_alt_hand.non_rotated_drawables.size() + m_primary_hand.non_rotated_drawables.size() + layers->size());
    append_prefix(result);
    result.append_range(std::move(*layers));
    Drawable::rebase_all(result);
    return result;
  }

  float timer = lerp(interpolation, m_previous_animation_timer, m_animation_timer);
  int base_frame = m_config.timing.state_seq(timer, m_state);
  auto arm_frame = arm_frame_for(base_frame);
  if (!arm_frame) return std::unexpected(std::move(arm_frame.error()));
  int body_frame = body_frame_for(base_frame);
  auto bob = bob_for_body_frame(body_frame);
  if (!bob) return std::unexpected(std::move(bob.error()));
  auto group = frame_base(m_state);
  if (!group) return std::unexpected(std::move(group.error()));

  Vec2F front_offset(0, *bob);
  if (front_hand.recoil) front_offset += m_config.recoil_offset;
  Vec2F back_offset(0, *bob);
  if (back_hand.recoil) back_offset += m_config.recoil_offset;
  Vec2F head_position(0, *bob);
  head_position += dance_step ? dance_step->get().head_offset / tile_pixels : base_head_offset();
  HumanoidImageLayers images{m_root, 1.f / tile_pixels};
  List<Drawable> drawables;
  append_prefix(drawables); // Legacy attachments bypass facing but share the final world transforms.
  auto add_drawable = [&](Drawable drawable, bool fullbright = false) -> Drawable& {
    if (m_facing_direction == Direction::Left) drawable.scale(Vec2F(-1, 1));
    drawable.fullbright |= fullbright;
    drawables.push_back(std::move(drawable));
    return drawables.back();
  };
  auto apply_head_rotation = [&](Drawable& drawable) {
    if (head_rotation != 0.f && with_rotation_and_scale) {
      float dir = numerical_direction(m_facing_direction);
      Vec2F pivot = head_position + m_config.head_rotation_center;
      pivot[0] *= dir;
      float head_x = head_rotation / (static_cast<float>(std::numbers::pi) * 2.f);
      drawable.rotate(head_rotation, pivot);
      drawable.position[0] -= state() == State::Run ? (std::fmaxf(head_x * dir, 0.f) * 2.f) * dir : head_x;
      drawable.position[1] -= std::fabsf(head_rotation / (static_cast<float>(std::numbers::pi) * 4.f));
    }
  };
  auto add_layer = [&](std::expected<Drawable, StarException> layer, bool fullbright, bool rotate_head = false)
      -> std::expected<void, StarException> {
    if (!layer) return std::unexpected(std::move(layer.error()));
    auto& placed = add_drawable(std::move(*layer), fullbright);
    if (rotate_head) apply_head_rotation(placed);
    return {};
  };
  auto body_image = [&](String const& frameset, Directives const& directives, bool backwards_back = false) {
    auto prefix = directives.prefix();
    if (dance_step && dance_step->get().body_frame)
      return Star::format("{}:{}{}", frameset, *dance_step->get().body_frame, prefix);
    if (m_state == Idle)
      return Star::format("{}:{}{}", frameset, m_identity.personality.idle, prefix);
    return Star::format("{}:{}.{}{}", frameset, backwards_back ? std::string_view("runbackwards") : *group, body_frame, prefix);
  };
  for (std::uint8_t i : fashion.worn_backs) {
    if (i == 0) break;
    auto const& back = std::get<WornBack>(fashion.wearables[std::size_t(i) - 1]);
    if (!back.frameset.empty()) {
      auto image = body_image(back.frameset, back.directives, m_moving_backwards && m_state == Run);
      auto added = add_layer(images.make(image, {}, {back.directives}), back.fullbright, back.rotate_with_head);
      if (!added) return std::unexpected(std::move(added.error()));
    }
  }

  auto draw_arm_and_sleeves = [&](bool front, bool holding) -> std::expected<void, StarException> {
    auto const& hand = front ? front_hand : back_hand;
    auto arm_layer = [&](String const& frameset, Directives const& directives, bool fullbright) -> std::expected<void, StarException> {
      auto prefix = directives.prefix();
      if (holding) {
        auto const& offset = front ? front_offset : back_offset;
        auto const& frame = front ? hand.front_frame : hand.back_frame;
        auto layer = images.make(Star::format("{}:{}{}", frameset, frame, prefix), offset,
                                 {directives, front ? hand.front_directives : hand.back_directives});
        if (!layer) return std::unexpected(std::move(layer.error()));
        if (front)
          layer->rotate(front_angle, offset + m_config.front_arm_rotation_center);
        else
          layer->rotate(back_angle, offset + m_config.back_arm_rotation_center + m_config.back_arm_offset);
        add_drawable(std::move(*layer), fullbright);
      } else {
        std::string image;
        Vec2F position;
        if (dance_step && (front ? dance_step->get().front_arm_frame : dance_step->get().back_arm_frame)) {
          auto const& step = dance_step->get();
          image = Star::format("{}:{}{}", frameset, *(front ? step.front_arm_frame : step.back_arm_frame), prefix);
          position = (front ? step.front_arm_offset : step.back_arm_offset) / tile_pixels;
        } else if (m_state == Idle) {
          image = Star::format("{}:{}{}", frameset, m_identity.personality.arm_idle, prefix);
          position = m_identity.personality.arm_offset / tile_pixels;
        } else {
          image = Star::format("{}:{}.{}{}", frameset, *group, *arm_frame, prefix);
        }
        auto layer = images.make(image, position, {directives});
        if (!layer) return std::unexpected(std::move(layer.error()));
        if (dance_step) layer->rotate(front ? dance_step->get().front_arm_rotation : dance_step->get().back_arm_rotation);
        add_drawable(std::move(*layer), fullbright);
      }
      return {};
    };
    auto const& arm = front ? m_front_arm_frameset : m_back_arm_frameset;
    if (!m_config.body_hidden && (holding || !arm.empty())) {
      if (auto placed = arm_layer(arm, get_body_directives(), m_config.body_fullbright); !placed) return placed;
    }
    for (std::uint8_t i : fashion.worn_chests_legs) {
      if (i == 0) break;
      auto const& wearable = fashion.wearables[std::size_t(i) - 1];
      if (!std::holds_alternative<WornChest>(wearable)) continue;
      auto const& chest = std::get<WornChest>(wearable);
      auto const& sleeve = front ? chest.front_sleeve_frameset : chest.back_sleeve_frameset;
      if (!sleeve.empty()) {
        if (auto placed = arm_layer(sleeve, chest.directives, chest.fullbright); !placed) return placed;
      }
    }
    return {};
  };
  bool back_holding = back_hand.holding_item && !dance_step && with_items;
  auto draw_back_item = [&] {
    auto angle = sample_angle(back_hand, back_primary, true);
    for (auto item : back_hand.item_drawables) {
      item.translate(m_config.front_hand_position + back_offset + m_config.back_arm_offset);
      item.rotate(angle, back_offset + m_config.back_arm_rotation_center + m_config.back_arm_offset);
      add_drawable(std::move(item));
    }
  };
  if (back_holding && !m_two_handed && back_hand.outside_of_hand) draw_back_item();
  if (auto placed = draw_arm_and_sleeves(false, back_holding); !placed) return std::unexpected(std::move(placed.error()));
  if (back_holding && !m_two_handed && !back_hand.outside_of_hand) draw_back_item();

  if (!m_head_frameset.empty() && !m_config.body_hidden) {
    auto placed = add_layer(images.make(Star::format("{}:normal", m_head_frameset), head_position, {get_body_directives()}), m_config.body_fullbright, true);
    if (!placed) return std::unexpected(std::move(placed.error()));
  }
  if (!m_emote_frameset.empty() && !m_config.body_hidden) {
    auto const& directives = get_emote_directives();
    auto frame = emote_descriptions[static_cast<std::size_t>(m_emote_state)].frame;
    int sequence = m_config.timing.emote_state_seq(lerp(interpolation, m_previous_emote_animation_timer, m_emote_animation_timer), m_emote_state);
    auto image = Star::format("{}:{}.{}{}", m_emote_frameset, frame, sequence, directives.prefix());
    auto placed = add_layer(images.make(image, head_position, {directives}), m_config.body_fullbright, true);
    if (!placed) return std::unexpected(std::move(placed.error()));
  }
  auto masked_face = [&](String const& frameset, Directives const& directives, bool fullbright) -> std::expected<void, StarException> {
    if (m_config.body_hidden || frameset.empty()) return {};
    return add_layer(images.make(Star::format("{}:normal", frameset), head_position, {directives}, fashion.helmet_mask_directives_group), fullbright, true);
  };
  if (auto placed = masked_face(m_hair_frameset, get_hair_directives(), m_config.body_fullbright); !placed) return std::unexpected(std::move(placed.error()));

  if (!m_body_frameset.empty() && !m_config.body_hidden) {
    auto const& directives = get_body_directives();
    auto image = body_image(m_body_frameset, directives);
    auto body = images.make(image, {}, {directives});
    if (!body) return std::unexpected(std::move(body.error()));
    if (m_config.use_body_mask && !m_body_mask_frameset.empty()) {
      Directives mask{String("?addmask=" + body_image(m_body_mask_frameset, directives) + ";0;0")};
      auto applied = body->image_part().add_directives(*images.metadata, mask, true);
      if (!applied) return std::unexpected(std::move(applied.error()));
    }
    add_drawable(std::move(*body), m_config.body_fullbright);
    if (m_config.use_body_head_mask && !m_body_head_mask_frameset.empty()) {
      Directives mask{String("?addmask=" + body_image(m_body_head_mask_frameset, directives) + ";0;0")};
      auto placed = add_layer(images.make(image, {}, {directives, mask}), m_config.body_fullbright, true);
      if (!placed) return std::unexpected(std::move(placed.error()));
    }
  }
  for (std::uint8_t i : fashion.worn_chests_legs) {
    if (i == 0) break;
    auto const& wearable = fashion.wearables[std::size_t(i) - 1];
    if (std::holds_alternative<WornLegs>(wearable)) {
      auto const& legs = std::get<WornLegs>(wearable);
      if (!legs.frameset.empty()) {
        auto placed = add_layer(images.make(body_image(legs.frameset, legs.directives), {}, {legs.directives}), legs.fullbright);
        if (!placed) return std::unexpected(std::move(placed.error()));
      }
    } else if (std::holds_alternative<WornChest>(wearable)) {
      auto const& chest = std::get<WornChest>(wearable);
      if (chest.frameset.empty()) continue;
      StringView frame;
      if (dance_step && dance_step->get().body_frame) frame = *dance_step->get().body_frame;
      else if (m_state == Run) frame = "run";
      else if (m_state == Idle) frame = m_identity.personality.idle;
      else if (m_state == Duck) frame = "duck";
      else if (m_state == Swim || m_state == SwimIdle) frame = "swim";
      else frame = "chest.1";
      auto image = Star::format("{}:{}{}", chest.frameset, frame, chest.directives.prefix());
      auto placed = add_layer(images.make(image, Vec2F(0, m_state == Duck ? 0.f : *bob), {chest.directives}), chest.fullbright);
      if (!placed) return std::unexpected(std::move(placed.error()));
    }
  }
  if (auto placed = masked_face(m_facial_hair_frameset, get_facial_hair_directives(), m_config.body_fullbright); !placed)
    return std::unexpected(std::move(placed.error()));
  if (auto placed = masked_face(m_facial_mask_frameset, get_facial_mask_directives(), false); !placed)
    return std::unexpected(std::move(placed.error()));
  for (std::uint8_t i : fashion.worn_heads) {
    if (i == 0) break;
    auto const& head = std::get<WornHead>(fashion.wearables[std::size_t(i) - 1]);
    if (!head.frameset.empty()) {
      auto image = Star::format("{}:normal{}", head.frameset, head.directives.prefix());
      auto placed = add_layer(images.make(image, head_position, {head.directives}), head.fullbright, true);
      if (!placed) return std::unexpected(std::move(placed.error()));
    }
  }

  bool front_holding = front_hand.holding_item && !dance_step && with_items;
  auto draw_front_item = [&] {
    auto angle = sample_angle(front_hand, front_primary, true);
    for (auto item : front_hand.item_drawables) {
      item.translate(m_config.front_hand_position + front_offset);
      item.rotate(angle, front_offset + m_config.front_arm_rotation_center);
      add_drawable(std::move(item));
    }
  };
  if (front_holding && !front_hand.outside_of_hand) draw_front_item();
  if (auto placed = draw_arm_and_sleeves(true, front_holding); !placed) return std::unexpected(std::move(placed.error()));
  if (front_holding && front_hand.outside_of_hand) draw_front_item();
  if (m_draw_vapor_trail) {
    auto sequence = m_config.timing.generic_seq(timer, m_config.vapor_trail_cycle, m_config.vapor_trail_frames, true);
    auto placed = add_layer(images.make(Star::format("{}:{}", m_vapor_trail_frameset, sequence), {}), false);
    if (!placed) return std::unexpected(std::move(placed.error()));
  }
  for (auto& drawable : drawables) {
    drawable.translate(m_config.global_offset);
    if (with_rotation_and_scale) {
      if (scale.x() != 1.f || scale.y() != 1.f) drawable.scale(scale);
      if (rotation != 0.f) drawable.rotate(rotation);
    }
    drawable.rebase();
  }
  return drawables;
}

std::expected<List<Drawable>, StarException> Humanoid::render_portrait(PortraitMode mode) const {
  auto& fashion = *m_fashion;
  refresh_wearables(fashion, *m_networked_animator);
  bool neutral = mode == PortraitMode::FullNeutral || mode == PortraitMode::FullNeutralNude;
  bool dressed = mode != PortraitMode::FullNude && mode != PortraitMode::FullNeutralNude;
  // Keep the selected species alive while borrowing its neutral personality.
  std::shared_ptr<SpeciesDefinition> species;
  std::reference_wrapper<Personality const> selected = std::cref(m_identity.personality);
  if (neutral) {
    auto database = m_root.speciesDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    auto found = (*database)->species(m_identity.species);
    if (!found) return std::unexpected(std::move(found.error()));
    species = std::move(*found);
    auto first = species->personalities().at(0);
    if (!first) return std::unexpected(std::move(first.error()));
    selected = first->get();
  }
  auto const& personality = selected.get();
  if (m_use_animation) {
    auto cloned = NetworkedAnimator::create(*m_networked_animator);
    if (!cloned) return std::unexpected(std::move(cloned.error()));
    auto& animator = **cloned;
    animator.set_flipped(false);
    animator.set_part_drawables(m_front_item_part, {});
    animator.set_part_drawables(m_back_item_part, {});
    if (auto reset = animator.reset_local_transformation_group("headRotation"); !reset) return std::unexpected(std::move(reset.error()));
    for (std::uint8_t i : fashion.worn_backs) {
      if (i == 0) break;
      auto reset = animator.reset_local_transformation_group("backCosmetic" + Star::to_string(i) + "Rotation");
      if (!reset) return std::unexpected(std::move(reset.error()));
    }
    if (auto reset = animator.reset_local_transformation_group("frontArmRotation"); !reset) return std::unexpected(std::move(reset.error()));
    if (auto reset = animator.reset_local_transformation_group("backArmRotation"); !reset) return std::unexpected(std::move(reset.error()));
    if (auto set = animator.set_local_state("frontArm", "idle"); !set) return std::unexpected(std::move(set.error()));
    if (auto set = animator.set_local_state("backArm", "idle"); !set) return std::unexpected(std::move(set.error()));
    if (auto states = m_portrait_animation_states.find_ref(mode)) {
      for (auto const& [name, args] : states->get()) {
        auto set = animator.set_local_state(name, args.state, args.start_new, args.reverse);
        if (!set) return std::unexpected(std::move(set.error()));
      }
    }
    if (neutral) {
      animator.set_local_tag("personalityIdle", personality.idle);
      animator.set_local_tag("personalityArmIdle", personality.arm_idle);
      auto reset_offset = [&](String const& group, Vec2F const& offset) -> std::expected<void, StarException> {
        if (auto reset = animator.reset_local_transformation_group(group); !reset) return reset;
        return animator.translate_local_transformation_group(group, offset / tile_pixels);
      };
      if (auto reset = reset_offset("personalityHeadOffset", personality.head_offset); !reset) return std::unexpected(std::move(reset.error()));
      if (auto reset = reset_offset("personalityArmOffset", personality.arm_offset); !reset) return std::unexpected(std::move(reset.error()));
    }
    if (!dressed) {
      animator.set_local_tag("helmetMaskDirectives");
      auto clear_tags = [&](WornAny const& worn) {
        for (auto const& tag : worn.animation_tags) animator.set_local_tag(tag.first);
      };
      for (auto const& wearable : fashion.wearables) {
        if (std::holds_alternative<WornHead>(wearable)) clear_tags(std::get<WornHead>(wearable));
        else if (std::holds_alternative<WornChest>(wearable)) clear_tags(std::get<WornChest>(wearable));
        else if (std::holds_alternative<WornLegs>(wearable)) clear_tags(std::get<WornLegs>(wearable));
        else if (std::holds_alternative<WornBack>(wearable)) clear_tags(std::get<WornBack>(wearable));
      }
    }
    if (auto updated = animator.update(0, {}); !updated) return std::unexpected(std::move(updated.error()));
    auto layers = animator.drawables();
    if (layers) Drawable::scale_all(*layers, tile_pixels);
    return layers;
  }

  HumanoidImageLayers images{m_root, 1.f};
  List<Drawable> drawables;
  std::optional<Directives> clip;
  if (!neutral && mode != PortraitMode::Full && mode != PortraitMode::FullNude)
    clip.emplace("addmask=/humanoid/portraitMask.png;0;0");
  auto add_layer = [&](String const& frameset, StringView frame, Vec2F const& position, Directives const& directives,
                       bool helmet_mask = false) -> std::expected<void, StarException> {
    if (frameset.empty()) return {};
    auto image = Star::format("{}:{}{}", frameset, frame, directives.prefix());
    auto layer = images.make(image, position, {directives}, helmet_mask ? std::optional(std::cref(fashion.helmet_mask_directives_group)) : std::nullopt);
    if (!layer) return std::unexpected(std::move(layer.error()));
    if (clip) {
      auto applied = layer->image_part().add_directives(*images.metadata, *clip, false);
      if (!applied) return std::unexpected(std::move(applied.error()));
    }
    drawables.push_back(std::move(*layer));
    return {};
  };
  auto sleeves = [&](bool front) -> std::expected<void, StarException> {
    for (std::uint8_t i : fashion.worn_chests_legs) {
      if (i == 0) break;
      auto const& wearable = fashion.wearables[std::size_t(i) - 1];
      if (!std::holds_alternative<WornChest>(wearable)) continue;
      auto const& chest = std::get<WornChest>(wearable);
      auto placed = add_layer(front ? chest.front_sleeve_frameset : chest.back_sleeve_frameset, personality.arm_idle, personality.arm_offset, chest.directives);
      if (!placed) return placed;
    }
    return {};
  };
  if (mode != PortraitMode::Head) {
    if (auto placed = add_layer(m_back_arm_frameset, personality.arm_idle, personality.arm_offset, get_body_directives()); !placed)
      return std::unexpected(std::move(placed.error()));
    if (dressed) {
      if (auto placed = sleeves(false); !placed) return std::unexpected(std::move(placed.error()));
      if (mode != PortraitMode::Bust) {
        for (std::uint8_t i : fashion.worn_backs) {
          if (i == 0) break;
          auto const& back = std::get<WornBack>(fashion.wearables[std::size_t(i) - 1]);
          if (auto placed = add_layer(back.frameset, personality.idle, {}, back.directives); !placed)
            return std::unexpected(std::move(placed.error()));
        }
      }
    }
  }
  if (auto placed = add_layer(m_head_frameset, "normal", personality.head_offset, get_body_directives()); !placed)
    return std::unexpected(std::move(placed.error()));
  if (!m_emote_frameset.empty()) {
    auto frame = emote_descriptions[static_cast<std::size_t>(m_emote_state)].frame;
    auto sequence = m_config.timing.emote_state_seq(m_emote_animation_timer, m_emote_state);
    auto name = Star::format("{}.{}", frame, sequence);
    if (auto placed = add_layer(m_emote_frameset, name, personality.head_offset, get_emote_directives()); !placed)
      return std::unexpected(std::move(placed.error()));
  }
  if (auto placed = add_layer(m_hair_frameset, "normal", personality.head_offset, get_hair_directives(), true); !placed)
    return std::unexpected(std::move(placed.error()));
  if (auto placed = add_layer(m_body_frameset, personality.idle, {}, get_body_directives()); !placed)
    return std::unexpected(std::move(placed.error()));
  if (mode != PortraitMode::Head && dressed) {
    for (std::uint8_t i : fashion.worn_chests_legs) {
      if (i == 0) break;
      auto const& wearable = fashion.wearables[std::size_t(i) - 1];
      if (std::holds_alternative<WornLegs>(wearable)) {
        auto const& legs = std::get<WornLegs>(wearable);
        if (auto placed = add_layer(legs.frameset, personality.idle, {}, legs.directives); !placed) return std::unexpected(std::move(placed.error()));
      } else if (std::holds_alternative<WornChest>(wearable)) {
        auto const& chest = std::get<WornChest>(wearable);
        if (auto placed = add_layer(chest.frameset, personality.idle, {}, chest.directives); !placed) return std::unexpected(std::move(placed.error()));
      }
    }
  }
  if (auto placed = add_layer(m_facial_hair_frameset, "normal", personality.head_offset, get_facial_hair_directives(), true); !placed)
    return std::unexpected(std::move(placed.error()));
  if (auto placed = add_layer(m_facial_mask_frameset, "normal", personality.head_offset, get_facial_mask_directives(), true); !placed)
    return std::unexpected(std::move(placed.error()));
  if (dressed) {
    for (std::uint8_t i : fashion.worn_heads) {
      if (i == 0) break;
      auto const& head = std::get<WornHead>(fashion.wearables[std::size_t(i) - 1]);
      if (auto placed = add_layer(head.frameset, "normal", personality.head_offset, head.directives); !placed)
        return std::unexpected(std::move(placed.error()));
    }
  }
  if (mode != PortraitMode::Head) {
    if (auto placed = add_layer(m_front_arm_frameset, personality.arm_idle, personality.arm_offset, get_body_directives()); !placed)
      return std::unexpected(std::move(placed.error()));
    if (dressed) {
      if (auto placed = sleeves(true); !placed) return std::unexpected(std::move(placed.error()));
    }
  }
  return drawables;
}

std::expected<List<Drawable>, StarException> Humanoid::render_skull() const {
  auto checked541 = m_root.image_metadata_database();
if (!checked541) return std::unexpected(std::move(checked541.error()));
auto checked542 = m_root.speciesDatabase();
if (!checked542) return std::unexpected(std::move(checked542.error()));
auto checked543 = std::move(*checked542)->species(m_identity.species);
if (!checked543) return std::unexpected(std::move(checked543.error()));
auto checked544 = Drawable::make_image(*std::move(*checked541),
      std::move(*checked543)->skull(), 1.0f, true, Vec2F());
if (!checked544) return std::unexpected(std::move(checked544.error()));
return List<Drawable>{std::move(*checked544)};
}

std::expected<std::shared_ptr<Humanoid>, StarException> Humanoid::make_dummy(Root& root, Gender) {
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
auto checked547 = std::move(*checked563).to_string();
if (!checked547) return std::unexpected(std::move(checked547.error()));
humanoid->m_head_frameset = std::move(*checked547);
  auto checked564 = assets->json("/humanoid/any/dummy.config:body");
if (!checked564) return std::unexpected(std::move(checked564.error()));
auto checked548 = std::move(*checked564).to_string();
if (!checked548) return std::unexpected(std::move(checked548.error()));
humanoid->m_body_frameset = std::move(*checked548);
  auto checked565 = assets->json("/humanoid/any/dummy.config:frontArm");
if (!checked565) return std::unexpected(std::move(checked565.error()));
auto checked549 = std::move(*checked565).to_string();
if (!checked549) return std::unexpected(std::move(checked549.error()));
humanoid->m_front_arm_frameset = std::move(*checked549);
  auto checked566 = assets->json("/humanoid/any/dummy.config:backArm");
if (!checked566) return std::unexpected(std::move(checked566.error()));
auto checked550 = std::move(*checked566).to_string();
if (!checked550) return std::unexpected(std::move(checked550.error()));
humanoid->m_back_arm_frameset = std::move(*checked550);
auto checked567 = assets->json("/humanoid/any/dummy.config:direction");
if (!checked567) return std::unexpected(std::move(checked567.error()));
auto checked551 = std::move(*checked567).to_string();
if (!checked551) return std::unexpected(std::move(checked551.error()));
auto checked552 = enum_value<direction_names>(*checked551);
if (!checked552) return std::unexpected(std::move(checked552.error()));
humanoid->set_facing_direction(*checked552);

return humanoid;
}

std::expected<List<Drawable>, StarException> Humanoid::render_dummy(Gender gender,
    std::optional<std::reference_wrapper<HeadArmor const>> head, std::optional<std::reference_wrapper<ChestArmor const>> chest,
    std::optional<std::reference_wrapper<LegsArmor const>> legs, std::optional<std::reference_wrapper<BackArmor const>> back) {
  auto fashion = std::exchange(m_fashion, std::make_shared<Fashion>());
  auto state = std::exchange(m_state, State::Idle);
  auto head_rotation = std::exchange(m_head_rotation, 0.f);
  auto restore = make_scope_exit([&] {
    m_fashion = std::move(fashion);
    m_state = state;
    m_head_rotation = head_rotation;
  });
  // All four preview slots start empty. Equipping them cannot remove any
  // logical animator tags; publish their tags to the private sample only.
  if (head)
    if (auto equipped = set_wearable_from_head(3, head->get(), gender); !equipped)
      return std::unexpected(std::move(equipped.error()));
  if (chest)
    if (auto equipped = set_wearable_from_chest(2, chest->get(), gender); !equipped)
      return std::unexpected(std::move(equipped.error()));
  if (legs)
    if (auto equipped = set_wearable_from_legs(1, legs->get(), gender); !equipped)
      return std::unexpected(std::move(equipped.error()));
  if (back)
    if (auto equipped = set_wearable_from_back(0, back->get(), gender); !equipped)
      return std::unexpected(std::move(equipped.error()));

  auto sample = m_networked_animator->sample(1.f);
  if (m_use_animation) {
    struct HideWornTags {
      NetworkedAnimator::Sample& sample;
      void operator()(std::monostate const&) const {}
      void operator()(WornAny const& worn) const {
        for (auto const& tag : worn.animation_tags)
          sample.set_local_tag(tag.first);
      }
    };
    for (auto const& worn : fashion->wearables)
      std::visit(HideWornTags{sample}, worn);
  }
  refresh_wearables(*m_fashion, sample);
  auto drawables = render(sample, false, false);
  if (!drawables) return std::unexpected(std::move(drawables.error()));
  Drawable::scale_all(*drawables, tile_pixels);
  return drawables;
}

std::expected<Vec2F, StarException> Humanoid::hand_position(ToolHand hand, Vec2F const& offset) const {
  auto local = hand_offset(hand, m_facing_direction);
  if (!local) return std::unexpected(std::move(local.error()));
  return arm_position(hand, m_facing_direction, get_hand(hand).angle, *local + offset);
}

std::expected<Vec2F, StarException> Humanoid::arm_position(ToolHand hand, Direction facing_direction, float arm_angle, Vec2F const& offset) const {
  bool front = hand == ToolHand::Primary ? (facing_direction == Direction::Left || m_two_handed)
                                        : facing_direction == Direction::Right;
  Vec2F position;
  if (m_use_animation) {
    auto const& animator = *m_networked_animator;
    auto const& rotation_point = front ? m_front_arm_rotation_point : m_back_arm_rotation_point;
    String arm_type = front ? String("frontArm") : String("backArm");
    std::optional<String> state = front ? m_primary_hand.front_frame : m_primary_hand.back_frame;
    auto has_state = animator.has_state(arm_type, state);
    if (!has_state) return std::unexpected(std::move(has_state.error()));
    if (!*has_state)
      state.emplace("rotation");
    auto property = [&](String const& key) {
      return animator.part_property(rotation_point.first, key, arm_type, state, 1);
    };
    auto center_json = property(rotation_point.second);
    if (!center_json) return std::unexpected(std::move(center_json.error()));
    auto center = json_to_vec2f(*center_json);
    if (!center) return std::unexpected(std::move(center.error()));
    auto anchor_json = property("anchorPart");
    if (!anchor_json) return std::unexpected(std::move(anchor_json.error()));
    auto anchor = anchor_json->to_string();
    if (!anchor) return std::unexpected(std::move(anchor.error()));
    auto groups_json = property("transformationGroups");
    if (!groups_json) return std::unexpected(std::move(groups_json.error()));
    auto group_names = json_to_string_list(*groups_json);
    if (!group_names) return std::unexpected(std::move(group_names.error()));

    // Replace only the first named arm rotation; without it, use the authored chain unchanged.
    std::span<String const> groups(*group_names);
    std::string_view rotation_group = front ? "frontArmRotation" : "backArmRotation";
    auto marker = std::find(groups.begin(), groups.end(), rotation_group);
    auto index = static_cast<std::size_t>(marker - groups.begin());
    auto transform = animator.group_transformation(groups.first(index));
    if (!transform) return std::unexpected(std::move(transform.error()));
    if (marker != groups.end()) {
      transform->rotate(arm_angle, *center);
      auto suffix = groups.subspan(index + 1);
      if (!suffix.empty()) {
        auto after = animator.group_transformation(suffix);
        if (!after) return std::unexpected(std::move(after.error()));
        *transform = *after * *transform;
      }
    }
    auto parent = animator.part_transformation(*anchor);
    if (!parent) return std::unexpected(std::move(parent.error()));
    position = (*parent * *transform).transform_point(offset + *center);
  } else {
    auto bob = bob_y_offset(1.f);
    if (!bob) return std::unexpected(std::move(bob.error()));
    if (!get_hand(hand).holding_item)
      return Vec2F();
    auto center = front ? m_config.front_arm_rotation_center + Vec2F(0, *bob)
                        : m_config.back_arm_rotation_center + Vec2F(0, *bob) + m_config.back_arm_offset;
    position = offset.rotate(arm_angle) + center;
  }
  if (facing_direction == Direction::Left)
    position[0] *= -1;
  return position;
}

std::expected<Vec2F, StarException> Humanoid::hand_offset(ToolHand hand, Direction facing_direction) const {
  bool const primary_in_front = facing_direction == Direction::Left || m_two_handed;
  bool const front = (hand == ToolHand::Primary) == primary_in_front;
  if (!m_use_animation)
    return m_config.front_hand_position - (front ? m_config.front_arm_rotation_center : m_config.back_arm_rotation_center);

  auto const& pivot = front ? m_front_arm_rotation_point : m_back_arm_rotation_point;
  auto item = m_networked_animator->part_property(front ? m_front_item_part : m_back_item_part, "offset");
  if (!item) return std::unexpected(std::move(item.error()));
  auto position = json_to_vec2f(*item);
  if (!position) return std::unexpected(std::move(position.error()));
  auto property = m_networked_animator->part_property(pivot.first, pivot.second);
  if (!property) return std::unexpected(std::move(property.error()));
  auto center = json_to_vec2f(*property);
  if (!center) return std::unexpected(std::move(center.error()));
  return *position - *center;
}

Humanoid::HandDrawingInfo const& Humanoid::get_hand(ToolHand hand) const {
  return hand == ToolHand::Primary ? m_primary_hand : m_alt_hand;
}

std::expected<std::string_view, StarException> Humanoid::frame_base(State state) const {
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
      if (!state_names) return std::unexpected(state_names.error());
      {
        auto name = state_names->get_right(state);
        if (!name) return std::unexpected(std::move(name.error()));
        return std::unexpected(StarException(Star::format("No such state '{}'", name->get())));
      }
  }
}


String Humanoid::get_head_from_identity() const {
  return Star::format("/humanoid/{}/{}head.png",
      m_identity.image_path ? *m_identity.image_path : m_identity.species,
      m_gender_name);
}

String Humanoid::get_body_from_identity() const {
  return Star::format("/humanoid/{}/{}body.png",
      m_identity.image_path ? *m_identity.image_path : m_identity.species,
      m_gender_name);
}

String Humanoid::get_body_mask_from_identity() const {
  return Star::format("/humanoid/{}/mask/{}body.png",
      m_identity.image_path ? *m_identity.image_path : m_identity.species,
      m_gender_name);
}

String Humanoid::get_body_head_mask_from_identity() const {
  return Star::format("/humanoid/{}/headmask/{}body.png",
      m_identity.image_path ? *m_identity.image_path : m_identity.species,
      m_gender_name);
}

String Humanoid::get_facial_emotes_from_identity() const {
  return Star::format("/humanoid/{}/emote.png", m_identity.image_path ? *m_identity.image_path : m_identity.species);
}

String Humanoid::get_hair_from_identity() const {
  if (m_identity.hair_type.empty())
    return "";
  return Star::format("/humanoid/{}/{}/{}.png",
      m_identity.image_path ? *m_identity.image_path : m_identity.species,
      m_identity.hair_group,
      m_identity.hair_type);
}

String Humanoid::get_facial_hair_from_identity() const {
  if (m_identity.facial_hair_type.empty())
    return "";
  return Star::format("/humanoid/{}/{}/{}.png",
      m_identity.image_path ? *m_identity.image_path : m_identity.species,
      m_identity.facial_hair_group,
      m_identity.facial_hair_type);
}

String Humanoid::get_facial_mask_from_identity() const {
  if (m_identity.facial_mask_type.empty())
    return "";
  return Star::format("/humanoid/{}/{}/{}.png",
      m_identity.image_path ? *m_identity.image_path : m_identity.species,
      m_identity.facial_mask_group,
      m_identity.facial_mask_type);
}

String Humanoid::get_back_arm_from_identity() const {
  return Star::format("/humanoid/{}/backarm.png", m_identity.image_path ? *m_identity.image_path : m_identity.species);
}

String Humanoid::get_front_arm_from_identity() const {
  return Star::format("/humanoid/{}/frontarm.png", m_identity.image_path ? *m_identity.image_path : m_identity.species);
}

String Humanoid::get_vapor_trail_frameset() const {
  return "/humanoid/any/flames.png";
}

Directives const& Humanoid::get_body_directives() const {
  return m_identity.body_directives;
}

Directives const& Humanoid::get_hair_directives() const {
  return m_identity.hair_directives;
}

Directives const& Humanoid::get_emote_directives() const {
  return m_identity.emote_directives;
}

Directives const& Humanoid::get_facial_hair_directives() const {
  return m_identity.facial_hair_directives;
}

Directives const& Humanoid::get_facial_mask_directives() const {
  return m_identity.facial_mask_directives;
}

DirectivesGroup const& Humanoid::get_helmet_mask_directives_group() const {
  return m_fashion->helmet_mask_directives_group;
}

std::expected<int, StarException> Humanoid::arm_frame_for(int base_frame) const {
  if (m_state != Walk && m_state != Run) return base_frame;
  auto const& sequence = m_state == Walk ? m_config.arm_walk_seq : m_config.arm_run_seq;
  auto frame = sequence.at(m_moving_backwards ? sequence.size() - base_frame : base_frame - 1);
  if (!frame) return std::unexpected(std::move(frame.error()));
  return frame->get();
}

int Humanoid::body_frame_for(int base_frame) const {
  if (m_moving_backwards && (m_state == Walk || m_state == Run))
    return m_config.timing.state_frames[m_state] - base_frame + 1;
  return base_frame;
}

std::expected<Humanoid::CachedDance, StarException> Humanoid::load_dance(String const& name) const {
  // Sample the generation first: a concurrent reload must never stamp an old definition as current.
  auto generation = m_root.dance_database_generation();
  auto database = m_root.dance_database();
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto definition = (*database)->get_dance(name);
  if (!definition)
    return std::unexpected(std::move(definition.error()));
  return CachedDance{std::move(*definition), generation};
}

std::expected<std::optional<std::reference_wrapper<Dance const>>, StarException> Humanoid::current_dance() const {
  if (!m_dance)
    return std::nullopt;
  if (m_cached_dance.generation != m_root.dance_database_generation()) {
    auto refreshed = load_dance(*m_dance);
    if (!refreshed)
      return std::unexpected(std::move(refreshed.error()));
    m_cached_dance = std::move(*refreshed);
  }
  return std::cref(*m_cached_dance.definition);
}

std::expected<std::optional<std::reference_wrapper<DanceStep const>>, StarException> Humanoid::sample_dance_step(float interpolation) const {
  if (!m_dance)
    return std::nullopt;
  return dance_step_at(lerp(interpolation, m_previous_dance_timer, m_dance_timer));
}

std::expected<std::optional<std::reference_wrapper<DanceStep const>>, StarException> Humanoid::dance_step_at(float timer) const {
  auto resolved = current_dance();
  if (!resolved)
    return std::unexpected(std::move(resolved.error()));
  if (!*resolved)
    return std::nullopt;
  if (!state_names)
    return std::unexpected(state_names.error());
  auto state_name = state_names->get_right(m_state);
  if (!state_name)
    return std::unexpected(std::move(state_name.error()));
  auto const& dance = (*resolved)->get();
  if (!dance.states.contains(state_name->get()))
    return std::nullopt;
  return dance.step_at(timer);
}

Humanoid::AnimationGroupRef Humanoid::movement_animation_group(State state, bool backwards) const {
  if (backwards)
    if (auto group = m_animation_states_backwards.find_ref(state))
      return group;
  return m_animation_states.find_ref(state);
}

std::expected<float, StarException> Humanoid::bob_y_offset(float interpolation) const {
  int frame = m_config.timing.state_seq(lerp(interpolation, m_previous_animation_timer, m_animation_timer), m_state);
  return bob_for_body_frame(body_frame_for(frame));
}

std::expected<float, StarException> Humanoid::bob_for_body_frame(int body_frame) const {
  switch (m_state) {
    case Run: {
      auto bob = m_config.run_bob.at(body_frame - 1);
      if (!bob) return std::unexpected(std::move(bob.error()));
      return m_config.run_fall_offset + bob->get();
    }
    case Walk:
    case Swim: {
      auto bob = (m_state == Walk ? m_config.walk_bob : m_config.swim_bob).at(body_frame - 1);
      if (!bob) return std::unexpected(std::move(bob.error()));
      return bob->get();
    }
    case Fall: return m_config.run_fall_offset;
    case Jump: return m_config.jump_bob;
    case Duck: return m_config.duck_offset;
    case Sit: return m_config.sit_offset;
    case Lay: return m_config.lay_offset;
    default: return 0.f;
  }
}

std::expected<Vec2F, StarException> Humanoid::arm_adjustment() const {
  auto checked639 = bob_y_offset(1.f);
if (!checked639) return std::unexpected(std::move(checked639.error()));
return Vec2F(0, std::move(*checked639));
}

std::expected<Vec2F, StarException> Humanoid::mouth_offset(bool ignore_adjustments) const {
  if (ignore_adjustments) {
    return (m_config.mouth_offset).rotate(m_rotation);
  } else {
    if (m_use_animation)
      {
auto checked640 = m_networked_animator->part_point(m_mouth_offset_point.first, m_mouth_offset_point.second);
if (!checked640) return std::unexpected(std::move(checked640.error()));
return std::move(*checked640).value_or(m_config.mouth_offset).rotate(m_rotation);
}

    auto checked641 = bob_y_offset(1.f);
if (!checked641) return std::unexpected(std::move(checked641.error()));
Vec2F head_position(0, std::move(*checked641));
    head_position += base_head_offset();

    return (m_config.mouth_offset + head_position).rotate(m_rotation);
  }
}

std::expected<Vec2F, StarException> Humanoid::feet_offset() const {
  if (m_use_animation)
    {
auto checked642 = m_networked_animator->part_point(m_feet_offset_point.first, m_feet_offset_point.second);
if (!checked642) return std::unexpected(std::move(checked642.error()));
return std::move(*checked642).value_or(m_config.feet_offset).rotate(m_rotation);
}
  return m_config.feet_offset.rotate(m_rotation);
}

std::expected<Vec2F, StarException> Humanoid::head_armor_offset() const {
  if (m_use_animation)
    {
auto checked643 = m_networked_animator->part_point(m_head_armor_offset_point.first, m_head_armor_offset_point.second);
if (!checked643) return std::unexpected(std::move(checked643.error()));
return std::move(*checked643).value_or(m_config.head_armor_offset).rotate(m_rotation);
}

  auto checked644 = bob_y_offset(1.f);
if (!checked644) return std::unexpected(std::move(checked644.error()));
Vec2F head_position(0, std::move(*checked644));
  head_position += base_head_offset();

  return (m_config.head_armor_offset + head_position).rotate(m_rotation);
}

std::expected<Vec2F, StarException> Humanoid::chest_armor_offset() const {
  if (m_use_animation)
    {
auto checked645 = m_networked_animator->part_point(m_chest_armor_offset_point.first, m_chest_armor_offset_point.second);
if (!checked645) return std::unexpected(std::move(checked645.error()));
return std::move(*checked645).value_or(m_config.chest_armor_offset).rotate(m_rotation);
}

  auto checked646 = bob_y_offset(1.f);
if (!checked646) return std::unexpected(std::move(checked646.error()));
Vec2F position(0, std::move(*checked646));
  return (m_config.chest_armor_offset + position).rotate(m_rotation);
}

std::expected<Vec2F, StarException> Humanoid::legs_armor_offset() const {
  if (m_use_animation)
    {
auto checked647 = m_networked_animator->part_point(m_legs_armor_offset_point.first, m_legs_armor_offset_point.second);
if (!checked647) return std::unexpected(std::move(checked647.error()));
return std::move(*checked647).value_or(m_config.legs_armor_offset).rotate(m_rotation);
}
  return m_config.legs_armor_offset.rotate(m_rotation);
}

std::expected<Vec2F, StarException> Humanoid::back_armor_offset() const {
  if (m_use_animation)
    {
auto checked648 = m_networked_animator->part_point(m_back_armor_offset_point.first, m_back_armor_offset_point.second);
if (!checked648) return std::unexpected(std::move(checked648.error()));
return std::move(*checked648).value_or(m_config.back_armor_offset).rotate(m_rotation);
}
  auto checked649 = bob_y_offset(1.f);
if (!checked649) return std::unexpected(std::move(checked649.error()));
Vec2F position(0, std::move(*checked649));
  return (m_config.back_armor_offset + position).rotate(m_rotation);
}

String Humanoid::default_death_particles() const {
  return m_config.default_death_particles;
}

std::expected<List<Particle>, StarException> Humanoid::particles(String const& name) const {
  auto checked650 = m_root.particleDatabase();
if (!checked650) return std::unexpected(std::move(checked650.error()));
auto particle_database = std::move(*checked650);
  List<Particle> res;
  auto checked651 = m_config.particle_emitters.get(name);
if (!checked651) return std::unexpected(std::move(checked651.error()));
auto checked652 = std::move(*checked651).get("particles", {});
if (!checked652) return std::unexpected(std::move(checked652.error()));
Json particles = std::move(*checked652);
  {
auto checked653 = particles.to_array();
if (!checked653) return std::unexpected(std::move(checked653.error()));
for (auto& particle : std::move(*checked653)) {
    auto checked654 = particle.get("particle", {});
if (!checked654) return std::unexpected(std::move(checked654.error()));
auto particle_spec = std::move(*checked654);
    auto checked655 = particle_database->particle(particle_spec);
if (!checked655) return std::unexpected(std::move(checked655.error()));
res.push_back(std::move(*checked655));
  }
}

  return res;
}

Json const& Humanoid::default_movement_parameters() const {
  return m_config.default_movement_parameters;
}
std::optional<Json> const& Humanoid::player_movement_parameters() const {
  return m_config.player_movement_parameters;
}

std::expected<std::pair<Vec2F, Directives>, StarException> Humanoid::extract_scale_from_directives(Directives const& directives) {
  if (!directives)
    return std::make_pair(Vec2F::filled(1.f), Directives());

  List<std::reference_wrapper<Directives::Entry const>> entries;
  std::size_t to_reserve = 0;
  std::optional<Vec2F> scale;

  for (auto const& entry : directives.entries()) {
    auto string = directives.entry_string(entry);
    auto scale_nearest = string.starts_with("scalenearest");
    if (!scale_nearest) return std::unexpected(std::move(scale_nearest.error()));
    if (*scale_nearest && string.utf8().find("skip") == npos) {
      auto const& operation = directives.entry_operation(entry);
      if (std::holds_alternative<ScaleImageOperation>(operation)) {
        auto const& op = std::get<ScaleImageOperation>(operation);
        scale = scale.value_or(Vec2F::filled(1.f)).piecewise_multiply(op.scale);
        continue;
      }
    }
    entries.emplace_back(std::cref(entry));
    to_reserve += string.utf8_size() + 1;
  }

  if (!scale)
    return std::make_pair(Vec2F::filled(1.f), directives);

  String merged_directives;
  merged_directives.reserve(to_reserve);
  for (auto const& entry : entries) {
    if (entry.get().begin > 0)
      merged_directives.append("?");
    merged_directives.append(directives.entry_string(entry.get()));
  }

  return std::make_pair(*scale, Directives(merged_directives));
}

std::optional<std::reference_wrapper<NetworkedAnimator>> Humanoid::networked_animator() {
  return optional_ref(m_networked_animator);
}
std::optional<std::reference_wrapper<NetworkedAnimator const>> Humanoid::networked_animator() const {
  if (!m_networked_animator)
    return std::nullopt;
  return std::cref(*m_networked_animator);
}

List<String> Humanoid::animation_scripts() const {
  return m_animation_scripts;
}

std::expected<Json, StarException> Humanoid::humanoid_config(bool with_overrides) {
  if (with_overrides)
    return json_merge(m_base_config, m_merge_config);
  return m_base_config;
}

NetHumanoid::NetHumanoid(Root& root) : m_root(root) {}

std::expected<std::shared_ptr<NetHumanoid>, StarException> NetHumanoid::create(Root& root) {
  return create(root, HumanoidIdentity(), JsonObject(), Json());
}

std::expected<std::shared_ptr<NetHumanoid>, StarException> NetHumanoid::create(Root& root, HumanoidIdentity identity, JsonObject parameters, Json config) {
  auto wrapper = std::shared_ptr<NetHumanoid>(new NetHumanoid(root));
  wrapper->m_config = config;
  auto humanoid = Humanoid::create(root, identity, parameters, config, std::nullopt);
  if (!humanoid) return std::unexpected(std::move(humanoid.error()));
  wrapper->m_humanoid_parameters.reset(std::move(parameters));
  wrapper->m_humanoid = std::move(*humanoid);
  auto elements = wrapper->setup_net_elements();
  if (!elements) return std::unexpected(std::move(elements.error()));
  return wrapper;
}

std::expected<void, StarException> NetHumanoid::net_store(DataStream& ds, NetCompatibilityRules rules) const {
  if (!check_with_rules(rules)) return {};
  auto fields = ds << m_humanoid->identity() << m_humanoid_parameters.base_map() << m_config;
  if (!fields) return std::unexpected(std::move(fields.error()));
  return NetElementSyncGroup::net_store(ds, rules);
}

std::expected<void, StarException> NetHumanoid::net_load(DataStream& ds, NetCompatibilityRules rules) {
  if (!check_with_rules(rules)) return {};
  HumanoidIdentity identity;
  JsonObject parameters;
  Json config;
  auto fields = ds >> identity >> parameters >> config;
  if (!fields) return std::unexpected(std::move(fields.error()));
  auto humanoid = Humanoid::create(m_root, identity, parameters, config, std::nullopt);
  if (!humanoid) return std::unexpected(std::move(humanoid.error()));
  m_config = std::move(config);
  m_humanoid_parameters.reset(std::move(parameters));
  m_humanoid = std::move(*humanoid);
  auto elements = setup_net_elements();
  if (!elements) return std::unexpected(std::move(elements.error()));
  return NetElementSyncGroup::net_load(ds, rules);
}

std::expected<void, StarException> NetHumanoid::net_elements_need_load(bool) {
  return {};
}

std::expected<void, StarException> NetHumanoid::net_elements_need_store() {
  if (m_humanoid_parameters.pull_updated())
    return m_humanoid->set_humanoid_parameters(m_humanoid_parameters.base_map());
  return {};
}

std::shared_ptr<Humanoid> NetHumanoid::humanoid() {
  return m_humanoid;
}

JsonObject NetHumanoid::humanoid_parameters() {
  return m_humanoid_parameters.base_map();
}

std::expected<void, StarException> NetHumanoid::set_humanoid_parameters(JsonObject parameters) {
  auto applied = m_humanoid->set_humanoid_parameters(parameters);
  if (!applied)
    return std::unexpected(std::move(applied.error()));
  m_humanoid_parameters.reset(std::move(parameters));
  m_humanoid_parameters.pull_updated(); // Already applied; no need to reparse on the next store.
  return {};
}

std::expected<void, StarException> NetHumanoid::setup_net_elements() {
  clear_net_elements();
  auto animator = add_net_element(m_humanoid->networked_animator()->get());
  if (!animator) return std::unexpected(std::move(animator.error()));
  return add_net_element(m_humanoid_parameters);
}
}
