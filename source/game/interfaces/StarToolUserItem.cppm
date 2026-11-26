module;
import std;
import star.type_identity;
import star.set;
import star.format;
import star.json;
import star.json_extra;
import star.id_map;
import star.casting;
import star.data_stream;
import star.game_types;
import star.poly;
import star.vector;
import star.strong_typedef;
import star.bi_map;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.rpc_promise;
import star.a_star;

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
import star.item;
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

namespace Star {

struct ToolUserItemExceptionTag {
  static constexpr std::string_view name() { return "ToolUserItemException"; }
};
using ToolUserItemException = StarError<ToolUserItemExceptionTag, StarException>;

// FIXME: You know what another name for an item that a tool user uses is?  A
// Tool.  Three words when one will do, rename.
class ToolUserItem {
public:
  virtual void query_type(TypeQuery& query) const noexcept { query_type_of<ToolUserItem>(*this, query); }
  virtual std::string_view dynamic_type_name() const noexcept { return Star::type_name<ToolUserItem>(); }

  ToolUserItem();
  virtual ~ToolUserItem() = default;

  // Owner must be initialized when a ToolUserItem is initialized, and the item
  // must be uninitialized before its owner is uninitialized.
  [[nodiscard]]
  virtual std::expected<void, StarException> init(ToolUserEntity& owner, ToolHand hand);
  [[nodiscard]]
  virtual std::expected<void, StarException> uninit();

  // Default implementation does nothing
  [[nodiscard]]
  virtual std::expected<void, StarException> update(float dt, FireMode fireMode, bool shifting, HashSet<MoveControlType> const& moves);

  // Default implementations return empty list
  [[nodiscard]]
  virtual std::expected<List<DamageSource>, StarException> damageSources() const;
  [[nodiscard]]
  virtual std::expected<List<PolyF>, StarException> shieldPolys() const;
  [[nodiscard]]
  virtual std::expected<List<PhysicsForceRegion>, StarException> force_regions() const;

  bool initialized() const;

  // Accessors report an error when initialized() is false.
  [[nodiscard]]
  std::expected<std::reference_wrapper<ToolUserEntity>, StarException> owner() const;
  [[nodiscard]]
  std::expected<EntityMode, StarException> entityMode() const;
  [[nodiscard]]
  std::expected<ToolHand, StarException> hand() const;
  [[nodiscard]]
  std::expected<std::reference_wrapper<World>, StarException> world() const;

protected:
  struct HandheldLight {
    Vec2F drawable_offset;
    Vec2F light_offset;
    Vec3F color;
    float point_beam;
    float beam_ambience;
  };

  struct AimedHand {
    Vec2F aim_position;
    Vec2F position;
    float angle;
  };

  // The two offsets in the returned configuration are in tiles.
  [[nodiscard]]
  static std::expected<HandheldLight, StarException> parse_handheld_light(Item const& item, float units_per_tile) {
    auto hand_position = item.instance_value("handPosition").and_then(json_to_vec2f);
    if (!hand_position)
      return std::unexpected(std::move(hand_position.error()));
    auto light_position = item.instance_value("lightPosition").and_then(json_to_vec2f);
    if (!light_position)
      return std::unexpected(std::move(light_position.error()));
    auto color = item.instance_value("lightColor").and_then(json_to_color);
    if (!color)
      return std::unexpected(std::move(color.error()));
    auto point_beam = item.instance_value("beamLevel").and_then([](Json const& value) { return value.to_float(); });
    if (!point_beam)
      return std::unexpected(std::move(point_beam.error()));
    auto beam_ambience = item.instance_value("beamAmbience").and_then([](Json const& value) { return value.to_float(); });
    if (!beam_ambience)
      return std::unexpected(std::move(beam_ambience.error()));
    return HandheldLight{-*hand_position / units_per_tile, (*light_position - *hand_position) / units_per_tile,
        color->to_rgb_f(), *point_beam, *beam_ambience};
  }

  [[nodiscard]]
  std::expected<AimedHand, StarException> aimed_hand(Vec2F const& offset) const {
    auto world_result = world();
    if (!world_result)
      return std::unexpected(std::move(world_result.error()));
    auto owner_result = owner();
    if (!owner_result)
      return std::unexpected(std::move(owner_result.error()));
    auto hand_result = hand();
    if (!hand_result)
      return std::unexpected(std::move(hand_result.error()));

    auto const& tool_owner = owner_result->get();
    auto aim_position = tool_owner.aim_position();
    if (!aim_position)
      return std::unexpected(std::move(aim_position.error()));
    auto position = tool_owner.hand_position(*hand_result, offset);
    if (!position)
      return std::unexpected(std::move(position.error()));
    auto owner_position = tool_owner.position();
    return AimedHand{*aim_position, owner_position + *position,
        world_result->get().geometry().diff(*aim_position, owner_position).angle()};
  }

  [[nodiscard]]
  std::expected<void, StarException> append_handheld_light(List<LightSource>& lights, HandheldLight const& light) const {
    if (!initialized())
      return {};
    auto pose = aimed_hand(light.light_offset);
    if (!pose)
      return std::unexpected(std::move(pose.error()));
    lights.emplace_back(pose->position, light.color, LightType::Point, light.point_beam, pose->angle, light.beam_ambience);
    return {};
  }

private:
  std::optional<std::reference_wrapper<ToolUserEntity>> m_owner;
  std::optional<ToolHand> m_hand;
};

}

export module star.tool_user_item;

export namespace Star {
  using ::Star::ToolUserItemExceptionTag;
  using ::Star::ToolUserItemException;
  using ::Star::ToolUserItem;
}
