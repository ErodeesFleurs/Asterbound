module;
import std;
import star.format;
import star.type_identity;
import star.string;
import star.json;
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

import star.actor_entity;
import star.status_types;
import star.particle;
import star.interaction_types;

namespace Star {

class Item;
class Input;
class ActorMovementController;
class StatusController;

// FIXME: This interface is a complete mess.
class ToolUserEntity : public virtual ActorEntity {
public:
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<ToolUserEntity, ActorEntity>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<ToolUserEntity>(); }

  virtual std::optional<std::reference_wrapper<Input>> input() const { return std::nullopt; }

  // Translates the given arm position into it's final entity space position
  // based on the given facing direction, and arm angle, and an offset from the
  // rotation center of the arm.
  [[nodiscard]] virtual std::expected<Vec2F, StarException> arm_position(ToolHand hand, Direction facing_direction, float arm_angle, Vec2F offset = {}) const = 0;
  // The offset to give to armPosition to get the position of the hand.
  [[nodiscard]] virtual std::expected<Vec2F, StarException> hand_offset(ToolHand hand, Direction facing_direction) const = 0;

  // Gets the world position of the current aim point.
  [[nodiscard]] virtual std::expected<Vec2F, StarException> aim_position() const = 0;

  virtual bool is_admin() const = 0;
  virtual Color favorite_color() const = 0;
  virtual String species() const = 0;

  [[nodiscard]] virtual std::expected<void, StarException> request_emote(String const& emote) = 0;

  // FIXME: This is effectively unusable, because since tool user items control
  // the angle and facing direction of the owner, and this uses the facing
  // direction and angle as input, the result will always be behind.
  [[nodiscard]] virtual std::expected<Vec2F, StarException> hand_position(ToolHand hand, Vec2F const& hand_offset = Vec2F()) const = 0;

  // FIXME: This was used for an Item to get a shared pointer to itself, which was
  // super bad and weird, but it COULD be used to get the item in the owner's
  // other hand, which is LESS bad.
  virtual std::shared_ptr<Item> hand_item(ToolHand hand) const = 0;

  // FIXME: What is the difference between interactRadius (which defines a tool
  // range) and inToolRange (which also defines a tool range indirectly).
  // inToolRange() implements based on the center of the tile of the aim
  // position (NOT the aim position!) but inToolRange(Vec2F) uses the given
  // position, which is again redundant.  Also, what is beamGunRadius and why
  // is it different than interact radius?  Can different tools have a
  // different interact radius?
  [[nodiscard]] virtual std::expected<float, StarException> interact_radius() const = 0;
  [[nodiscard]] virtual std::expected<bool, StarException> in_tool_range() const = 0;
  [[nodiscard]] virtual std::expected<bool, StarException> in_tool_range(Vec2F const& position) const = 0;
  [[nodiscard]] virtual std::expected<float, StarException> beam_gun_radius() const = 0;

  // FIXME: Too specific to Player, just cast to Player if you have to and do
  // that, NPCs cannot possibly implement these properly (and do not implement
  // them at all).
  [[nodiscard]] virtual std::expected<void, StarException> queue_ui_message(String const& message) = 0;
  [[nodiscard]] virtual std::expected<void, StarException> interact(InteractAction action) = 0;

  // FIXME: Ditto here, instrumentPlaying() is just an accessor to the songbook
  // for when the songbook has had a song selected, and the instrument decides
  // when to cancel music anyway, also instrumentEquipped(String) is a straight
  // up ridiculous way of notifying the Player that the player itself is
  // holding an instrument, which it already knows.
  [[nodiscard]] virtual std::expected<bool, StarException> instrument_playing() = 0;
  [[nodiscard]] virtual std::expected<void, StarException> instrument_equipped(String const& instrumentKind) = 0;

  // FIXME: how is this related to the hand position and isn't it already
  // included in the hand position and why is it necessary?
  [[nodiscard]] virtual std::expected<Vec2F, StarException> arm_adjustment() const = 0;

  // FIXME: These were all fine, just need to be fixed because now we have the
  // movement controller itself and can use that directly
  Vec2F position() const override = 0;
  virtual Vec2F velocity() const = 0;
  virtual Direction facing_direction() const = 0;
  virtual Direction walking_direction() const = 0;

  // FIXME: Ditto here, except we now have the status controller directly.
  [[nodiscard]] virtual std::expected<float, StarException> power_multiplier() const = 0;
  [[nodiscard]] virtual std::expected<bool, StarException> full_energy() const = 0;
  [[nodiscard]] virtual std::expected<float, StarException> energy() const = 0;
  [[nodiscard]] virtual std::expected<bool, StarException> consume_energy(float energy) = 0;
  [[nodiscard]] virtual std::expected<bool, StarException> energy_locked() const = 0;
  [[nodiscard]] virtual std::expected<void, StarException> add_ephemeral_status_effects(List<EphemeralStatusEffect> const& statusEffects) = 0;
  [[nodiscard]] virtual std::expected<ActiveUniqueStatusEffectSummary, StarException> active_unique_status_effect_summary() const = 0;

  // FIXME: This is a dumb way of getting limited animation support
  [[nodiscard]] virtual std::expected<void, StarException> add_effect_emitters(StringSet const& emitters) = 0;
  [[nodiscard]] virtual std::expected<void, StarException> add_particles(List<Particle> const& particles) = 0;
  [[nodiscard]] virtual std::expected<void, StarException> add_sound(String const& sound, float volume = 1.0f, float pitch = 1.0f) = 0;

  virtual void set_camera_focus_entity(std::optional<EntityId> const& cameraFocusEntity) = 0;
};

}

export module star.tool_user_entity;

export namespace Star {
  using ::Star::Item;
  using ::Star::ToolUserEntity;
  using ::Star::ActorMovementController;
  using ::Star::StatusController;
}
