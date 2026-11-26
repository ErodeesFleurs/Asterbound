import std;
import star.format;
import star.string;
import star.json;
import star.id_map;
import star.casting;
import star.data_stream;
import star.game_types;
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
import star.lua_root;
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
import star.status_effect_item;
import star.lua_components;
import star.fireable_item;
import star.swingable_item;

import star.math_common;
namespace Star {

SwingableItem::SwingableItem() {
  m_swingAimFactor = 0;
  m_swingStart = 0;
  m_swingFinish = 0;
}

std::expected<void, StarException> SwingableItem::initializeSwingable(Json const& params) {
  auto initialized = initializeFireable(params);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return setParams(params);
}

std::expected<void, StarException> SwingableItem::setParams(Json const& params) {
  auto start = params.get_float("swingStart", 60);
  if (!start)
    return std::unexpected(std::move(start.error()));
  m_swingStart = *start * std::numbers::pi / 180;
  auto finish = params.get_float("swingFinish", -40);
  if (!finish)
    return std::unexpected(std::move(finish.error()));
  m_swingFinish = *finish * std::numbers::pi / 180;
  auto factor = params.get_float("swingAimFactor", 1);
  if (!factor)
    return std::unexpected(std::move(factor.error()));
  m_swingAimFactor = *factor;
  auto coolingDown = params.opt_float("coolingDownAngle");
  if (!coolingDown)
    return std::unexpected(std::move(coolingDown.error()));
  m_coolingDownAngle = coolingDown->transform([](float angle) { return angle * std::numbers::pi / 180; });
  return FireableItem::setParams(params);
}

std::expected<float, StarException> SwingableItem::getAngleDir(float angle, Direction) {
  return getAngle(angle);
}

std::expected<float, StarException> SwingableItem::getAngle(float aimAngle) {
  if (!ready()) {
    if (coolingDown()) {
      if (m_coolingDownAngle)
        return *m_coolingDownAngle + aimAngle * m_swingAimFactor;
      else
        return -std::numbers::pi / 2;
    }

    if (m_timeFiring < windupTime())
      return m_swingStart + (m_swingFinish - m_swingStart) * m_timeFiring / windupTime() + aimAngle * m_swingAimFactor;

    return m_swingFinish + (m_swingStart - m_swingFinish) * fireTimer() / (cooldown_time() + windupTime()) + aimAngle * m_swingAimFactor;
  }

  return -std::numbers::pi / 2;
}

std::expected<float, StarException> SwingableItem::getItemAngle(float aimAngle) {
  return getAngle(aimAngle);
}

String SwingableItem::getArmFrame() {
  return "rotation";
}

}
