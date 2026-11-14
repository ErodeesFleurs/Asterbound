module;
import std;
import star.format;
import star.type_identity;
import star.string;
import star.set;
import star.json;
import star.lua_components;
import star.id_map;
import star.casting;
import star.vector;
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

import star.status_effect_item;

namespace Star {

class FireableItem : public virtual ToolUserItem, public virtual StatusEffectItem {
public:
  void const* query_type(TypeKey key) const noexcept override {
    return query_type_of<FireableItem, ToolUserItem, StatusEffectItem>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<FireableItem>(); }

  FireableItem();
  ~FireableItem() override = default;

  FireableItem(FireableItem const& fireableItem);

  [[nodiscard]]
  virtual std::expected<void, StarException> fire(FireMode mode, bool shifting, bool edgeTriggered);
  [[nodiscard]]
  virtual std::expected<void, StarException> endFire(FireMode mode, bool shifting);
  virtual FireMode fireMode() const;
  virtual float fireTimer() const;
  virtual void setFireTimer(float fireTimer);
  virtual float cooldownTime() const;
  virtual void setCooldownTime(float cooldownTime);
  virtual float windupTime() const;
  virtual void setWindupTime(float time);
  virtual bool ready() const;
  virtual bool firing() const;
  virtual bool inUse() const;
  virtual bool walkWhileFiring() const;
  virtual bool stopWhileFiring() const;
  [[nodiscard]]
  virtual std::expected<bool, StarException> windup() const;
  [[nodiscard]]
  virtual std::expected<void, StarException> triggerCooldown();
  virtual bool coolingDown() const;
  virtual void setCoolingDown(bool coolingdown);
  virtual float timeFiring() const;
  virtual void setTimeFiring(float timeFiring);
  virtual Vec2F firePosition() const;
  virtual Vec2F handPosition() const;

  [[nodiscard]]
  std::expected<void, StarException> init(ToolUserEntity& owner, ToolHand hand) override;
  [[nodiscard]]
  std::expected<void, StarException> uninit() override;
  [[nodiscard]]
  std::expected<void, StarException> update(float dt, FireMode fireMode, bool shifting,
                                            HashSet<MoveControlType> const& moves) override;

  [[nodiscard]]
  std::expected<List<PersistentStatusEffect>, StarException> statusEffects() const override;

  virtual bool validAimPos(Vec2F const& aimPos);

  [[nodiscard]]
  std::expected<Json, StarException> fireableParam(String const& key) const;
  [[nodiscard]]
  std::expected<Json, StarException> fireableParam(String const& key, Json const& defaultVal) const;

protected:
  [[nodiscard]]
  std::expected<void, StarException> initializeFireable(Json const& params);
  [[nodiscard]]
  std::expected<void, StarException> setParams(Json const& params);
  [[nodiscard]]
  std::expected<void, StarException> setFireableParam(String const& key, Json const& value);
  [[nodiscard]]
  virtual std::expected<void, StarException> startTriggered();
  [[nodiscard]]
  virtual std::expected<void, StarException> fireTriggered();

  // firePosition translated by the hand in the owner's space
  [[nodiscard]]
  std::expected<Vec2F, StarException> ownerFirePosition() const;

  float m_fireTimer;
  float m_cooldownTime;
  float m_windupTime;
  bool m_fireWhenReady;
  bool m_startWhenReady;
  bool m_cooldown;
  bool m_alreadyInit;
  bool m_requireEdgeTrigger;

  bool m_attemptedFire;
  bool m_fireOnRelease;
  float m_timeFiring;
  bool m_startTimingFire;
  bool m_inUse;
  bool m_walkWhileFiring;
  bool m_stopWhileFiring;

  mutable std::optional<LuaWorldScript> m_scriptComponent;

  Json m_fireableParams;

  Vec2F m_handPosition;

  FireMode m_mode;
};

}

export module star.fireable_item;

export namespace Star {
  using ::Star::FireableItem;
}
