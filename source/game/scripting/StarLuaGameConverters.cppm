module;
import std;
import star.format;
import star.lua;
import star.lua_converters;
import star.inventory_types;
import star.collision_block;
import star.platformer_astar_types;
import star.status_types;

namespace Star {

class Entity;
class AudioInstance;
struct ActorJumpProfile;
struct ActorMovementParameters;
struct ActorMovementModifiers;
struct DamageRequest;
struct DamageNotification;
struct LiquidLevel;
struct Drawable;
struct PhysicsMovingCollision;

struct Collection;
struct Collectable;
class BehaviorState;
class Blackboard;
enum class NodeStatus;

template <>
struct LuaConverter<InventorySlot> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, InventorySlot k);
  [[nodiscard]] static std::expected<std::optional<InventorySlot>, StarException> to(LuaStackValue v);
};

template <>
struct LuaConverter<CollisionKind> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, CollisionKind k);
  [[nodiscard]] static std::expected<std::optional<CollisionKind>, StarException> to(LuaStackValue v);
};

template <>
struct LuaConverter<CollisionSet> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, CollisionSet const& s);
  [[nodiscard]] static std::expected<std::optional<CollisionSet>, StarException> to(LuaStackValue v);
};

template <>
struct LuaConverter<PlatformerAStar::Path> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, PlatformerAStar::Path const& path);
};

template <>
struct LuaConverter<std::shared_ptr<PlatformerAStar::PathFinder>> : LuaUserDataConverter<std::shared_ptr<PlatformerAStar::PathFinder>> {};

template <>
struct LuaUserDataMethods<std::shared_ptr<PlatformerAStar::PathFinder>> {
  [[nodiscard]] static std::expected<LuaMethods<std::shared_ptr<PlatformerAStar::PathFinder>>, StarException> make();
};

template <>
struct LuaConverter<PlatformerAStar::Parameters> {
  [[nodiscard]] static std::expected<std::optional<PlatformerAStar::Parameters>, StarException> to(LuaStackValue v);
};

template <>
struct LuaConverter<ActorJumpProfile> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, ActorJumpProfile const& v);
  [[nodiscard]] static std::expected<std::optional<ActorJumpProfile>, StarException> to(LuaStackValue v);
};

template <>
struct LuaConverter<ActorMovementParameters> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, ActorMovementParameters const& v);
  [[nodiscard]] static std::expected<std::optional<ActorMovementParameters>, StarException> to(LuaStackValue v);
};

template <>
struct LuaConverter<ActorMovementModifiers> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, ActorMovementModifiers const& v);
  [[nodiscard]] static std::expected<std::optional<ActorMovementModifiers>, StarException> to(LuaStackValue v);
};

template <>
struct LuaConverter<StatModifier> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, StatModifier const& v);
  [[nodiscard]] static std::expected<std::optional<StatModifier>, StarException> to(LuaStackValue v);
};

template <>
struct LuaConverter<EphemeralStatusEffect> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, EphemeralStatusEffect const& v);
  [[nodiscard]] static std::expected<std::optional<EphemeralStatusEffect>, StarException> to(LuaStackValue v);
};

template <>
struct LuaConverter<DamageRequest> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, DamageRequest const& v);
  [[nodiscard]] static std::expected<std::optional<DamageRequest>, StarException> to(LuaStackValue v);
};

template <>
struct LuaConverter<DamageNotification> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, DamageNotification const& v);
  [[nodiscard]] static std::expected<std::optional<DamageNotification>, StarException> to(LuaStackValue v);
};

template <>
struct LuaConverter<LiquidLevel> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, LiquidLevel const& v);
  [[nodiscard]] static std::expected<std::optional<LiquidLevel>, StarException> to(LuaStackValue v);
};

template <>
struct LuaConverter<Drawable> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, Drawable const& v);
  [[nodiscard]] static std::expected<std::optional<Drawable>, StarException> to(LuaStackValue v);
};

template <>
struct LuaConverter<Collection> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, Collection const& c);
  [[nodiscard]] static std::expected<std::optional<Collection>, StarException> to(LuaStackValue v);
};

template <>
struct LuaConverter<Collectable> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, Collectable const& c);
  [[nodiscard]] static std::expected<std::optional<Collectable>, StarException> to(LuaStackValue v);
};

// BehaviorState contains Lua references, putting it in a UserData violates
// the "don't put lua references in userdata, just don't" rule. We get around it by keeping
// a weak pointer to the behavior state, forcing it to be destroyed elsewhere.
template <>
struct LuaConverter<std::weak_ptr<BehaviorState>> : LuaUserDataConverter<std::weak_ptr<BehaviorState>> {};

template <>
struct LuaUserDataMethods<std::weak_ptr<BehaviorState>> {
  [[nodiscard]] static std::expected<LuaMethods<std::weak_ptr<BehaviorState>>, StarException> make();
};

template <>
struct LuaConverter<NodeStatus> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, NodeStatus const& status);
  [[nodiscard]] static std::expected<NodeStatus, StarException> to(LuaStackValue v);
};

template <>
struct LuaConverter<PhysicsMovingCollision> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, PhysicsMovingCollision const& v);
};

// Weak pointer for the same reasons as BehaviorState.
template <>
struct LuaConverter<std::weak_ptr<Blackboard>> : LuaUserDataConverter<std::weak_ptr<Blackboard>> {};

template <>
struct LuaUserDataMethods<std::weak_ptr<Blackboard>> {
  [[nodiscard]] static std::expected<LuaMethods<std::weak_ptr<Blackboard>>, StarException> make();
};

template <>
struct LuaConverter<std::shared_ptr<Entity>> : LuaUserDataConverter<std::shared_ptr<Entity>> {};

template <>
struct LuaUserDataMethods<std::shared_ptr<Entity>> {
  [[nodiscard]] static std::expected<LuaMethods<std::shared_ptr<Entity>>, StarException> make();
};

template <>
struct LuaConverter<std::shared_ptr<AudioInstance>> : LuaUserDataConverter<std::shared_ptr<AudioInstance>> {};

template <>
struct LuaUserDataMethods<std::shared_ptr<AudioInstance>> {
  [[nodiscard]] static std::expected<LuaMethods<std::shared_ptr<AudioInstance>>, StarException> make();
};

}

export module star.lua_game_converters;

export namespace Star {
  template <>
  struct LuaConverter<InventorySlot>;
  template <>
  struct LuaConverter<CollisionKind>;
  template <>
  struct LuaConverter<CollisionSet>;
  template <>
  struct LuaConverter<PlatformerAStar::Path>;
  template <>
  struct LuaConverter<std::shared_ptr<PlatformerAStar::PathFinder>>;
  template <>
  struct LuaUserDataMethods<std::shared_ptr<PlatformerAStar::PathFinder>>;
  template <>
  struct LuaConverter<PlatformerAStar::Parameters>;
  template <>
  struct LuaConverter<ActorJumpProfile>;
  template <>
  struct LuaConverter<ActorMovementParameters>;
  template <>
  struct LuaConverter<ActorMovementModifiers>;
  template <>
  struct LuaConverter<StatModifier>;
  template <>
  struct LuaConverter<EphemeralStatusEffect>;
  template <>
  struct LuaConverter<DamageRequest>;
  template <>
  struct LuaConverter<DamageNotification>;
  template <>
  struct LuaConverter<LiquidLevel>;
  template <>
  struct LuaConverter<Drawable>;
  template <>
  struct LuaConverter<Collection>;
  template <>
  struct LuaConverter<Collectable>;
  template <>
  struct LuaConverter<std::weak_ptr<BehaviorState>>;
  template <>
  struct LuaUserDataMethods<std::weak_ptr<BehaviorState>>;
  template <>
  struct LuaConverter<NodeStatus>;
  template <>
  struct LuaConverter<PhysicsMovingCollision>;
  template <>
  struct LuaConverter<std::weak_ptr<Blackboard>>;
  template <>
  struct LuaUserDataMethods<std::weak_ptr<Blackboard>>;
  template <>
  struct LuaConverter<std::shared_ptr<Entity>>;
  template <>
  struct LuaUserDataMethods<std::shared_ptr<Entity>>;
  template <>
  struct LuaConverter<std::shared_ptr<AudioInstance>>;
  template <>
  struct LuaUserDataMethods<std::shared_ptr<AudioInstance>>;

  using ::Star::LuaConverter;
  using ::Star::LuaUserDataMethods;
}
