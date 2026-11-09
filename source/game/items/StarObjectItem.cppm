module;
import std;
import star.type_identity;
import star.json;
import star.json_extra;
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
import star.spline;
import star.string;
import star.logging;
import star.set;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.listener;
import std;
import star.format;
import star.version;
import star.random;
import star.periodic;
import star.periodic_function;
import star.lua;
import star.audio;
import star.map;
import star.lua_root;
import star.lua_components;
import star.lua_animation_component;


import star.item;
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
import star.fireable_item;
import star.drawable;
import star.non_rotated_drawables_item;
import star.beam_item;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.output_proxy;
import star.root;
import star.status_effect_entity;
import star.scripted_entity;
import star.chat_action;
import star.chatty_entity;
import star.wiring;
import star.wire_entity;
import star.inspectable_entity;
import star.animated_part_set;
import star.mixer;
import star.networked_animator;
import star.entity_rendering_types;
import star.entity_rendering;
import star.object;

export module star.object_item;
import star.object_database;


export namespace Star {

class ObjectItem : public Item, public FireableItem, public BeamItem {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<ObjectItem, Item, FireableItem, BeamItem>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<ObjectItem>();
  }

  [[nodiscard]]
  static std::expected<std::shared_ptr<ObjectItem>, StarException> create(Root& root, Json const& config, String const& directory, Json const& objectParameters);
  ~ObjectItem() override = default;

  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> clone() const override;

  [[nodiscard]]
  std::expected<void, StarException> init(ToolUserEntity& owner, ToolHand hand) override;
  [[nodiscard]]
  std::expected<void, StarException> update(float dt, FireMode fireMode, bool shifting, HashSet<MoveControlType> const& moves) override;

  [[nodiscard]]
  std::expected<bool, StarException> beamCanPlace() const override;

  float cooldownTime() const override;
  [[nodiscard]]
  std::expected<void, StarException> fire(FireMode mode, bool shifting, bool edgeTriggered) override;

  [[nodiscard]]
  std::expected<String, StarException> objectName() const;
  [[nodiscard]]
  std::expected<Json, StarException> objectParameters() const;

  [[nodiscard]]
  std::expected<bool, StarException> placeInWorld(FireMode mode, bool shifting);
  [[nodiscard]]
  std::expected<bool, StarException> canPlace(bool shifting) const;

private:
  explicit ObjectItem(Root& root) : Item(root), BeamItem(root) {}
  bool m_shifting;
};

}

namespace Star {

std::expected<std::shared_ptr<ObjectItem>, StarException> ObjectItem::create(Root& root, Json const& config, String const& directory, Json const& objectParameters) {
  auto item = std::shared_ptr<ObjectItem>(new ObjectItem(root));
  auto initialized = item->initialize(config, directory, objectParameters);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto fireable = item->initializeFireable(config);
  if (!fireable)
    return std::unexpected(std::move(fireable.error()));
  auto beam = item->initializeBeam(config);
  if (!beam)
    return std::unexpected(std::move(beam.error()));
  auto twoHanded = config.getBool("twoHanded", true);
  if (!twoHanded)
    return std::unexpected(std::move(twoHanded.error()));
  item->setTwoHanded(*twoHanded);
  // Retained script storage participates in the existing item stacking contract.
  auto retain = item->instanceValue("retainObjectParametersInItem", false).and_then([](Json const& value) { return value.toBool(); });
  if (!retain)
    return std::unexpected(std::move(retain.error()));
  if (*retain) {
    auto storage = item->instanceValue("scriptStorage");
    if (!storage)
      return std::unexpected(std::move(storage.error()));
    if (storage->isNull()) {
      auto stored = item->setInstanceValue("scriptStorage", JsonObject());
      if (!stored)
        return std::unexpected(std::move(stored.error()));
    }
  }
  item->m_shifting = false;
  return item;
}

std::expected<std::shared_ptr<Item>, StarException> ObjectItem::clone() const {
  return std::make_shared<ObjectItem>(*this);
}

std::expected<void, StarException> ObjectItem::init(ToolUserEntity& owner, ToolHand hand) {
  auto initialized = FireableItem::init(owner, hand);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return BeamItem::init(owner, hand);
}

std::expected<void, StarException> ObjectItem::update(float dt, FireMode fireMode, bool shifting, HashSet<MoveControlType> const& moves) {
  auto fireable = FireableItem::update(dt, fireMode, shifting, moves);
  if (!fireable)
    return std::unexpected(std::move(fireable.error()));
  setEnd(BeamItem::EndType::Object);
  m_shifting = shifting;
  auto beam = BeamItem::update(dt, fireMode, shifting, moves);
  if (!beam)
    return std::unexpected(std::move(beam.error()));
  return {};
}

std::expected<bool, StarException> ObjectItem::beamCanPlace() const {
  return canPlace(m_shifting);
}

float ObjectItem::cooldownTime() const {
  // TODO: Hardcoded
  return 0.25f;
}

std::expected<void, StarException> ObjectItem::fire(FireMode mode, bool shifting, bool edgeTriggered) {
  if (!ready())
    return {};
  auto placed = placeInWorld(mode, shifting);
  if (!placed)
    return std::unexpected(std::move(placed.error()));
  if (*placed)
    return FireableItem::fire(mode, shifting, edgeTriggered);
  return {};
}

std::expected<String, StarException> ObjectItem::objectName() const {
  return instanceValue("objectName", "<objectName missing>").and_then([](Json const& value) { return value.toString(); });
}

std::expected<Json, StarException> ObjectItem::objectParameters() const {
  Json objectParameters = parameters().opt().value_or(JsonObject{});
  if (!initialized())
    return objectParameters;
  auto itemOwner = owner();
  if (!itemOwner)
    return std::unexpected(std::move(itemOwner.error()));
  return objectParameters.set("owner", jsonFromOptional(itemOwner->get().uniqueId()));
}

std::expected<bool, StarException> ObjectItem::placeInWorld(FireMode, bool shifting) {
  if (!initialized())
    return std::unexpected(ItemException("ObjectItem not init'd properly, or user not recognized as Tool User."));
  if (!ready())
    return false;
  auto placeable = canPlace(shifting);
  if (!placeable)
    return std::unexpected(std::move(placeable.error()));
  if (!*placeable)
    return false;
  auto itemOwner = owner();
  if (!itemOwner)
    return std::unexpected(std::move(itemOwner.error()));
  auto itemWorld = world();
  if (!itemWorld)
    return std::unexpected(std::move(itemWorld.error()));
  auto aimPosition = itemOwner->get().aimPosition();
  if (!aimPosition)
    return std::unexpected(std::move(aimPosition.error()));
  auto pos = Vec2I(aimPosition->floor());
  auto database = root().objectDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto attempted = [&]() -> std::expected<bool, StarException> {
    auto name = objectName();
    if (!name)
      return std::unexpected(std::move(name.error()));
    auto parameters = objectParameters();
    if (!parameters)
      return std::unexpected(std::move(parameters.error()));
    auto object = (*database)->createForPlacement(itemWorld->get(), *name, pos, itemOwner->get().walkingDirection(), *parameters);
    if (!object)
      return std::unexpected(std::move(object.error()));
    if (*object && consume(1)) {
      auto added = itemWorld->get().addEntity(*object);
      if (!added)
        return std::unexpected(std::move(added.error()));
      return true;
    }
    return false;
  }();
  if (attempted)
    return *attempted;
  auto name = objectName();
  if (!name)
    return std::unexpected(std::move(name.error()));
  auto logged = root().logger().errorException(Star::format("Failed to instantiate object '{}' for placement", *name), ExceptionDiagnostic(attempted.error()));
  if (!logged)
    return std::unexpected(std::move(logged.error()));
  if (root().logger().loggable(LogLevel::Debug)) {
    auto parameters = objectParameters();
    if (!parameters)
      return std::unexpected(std::move(parameters.error()));
    auto representation = parameters->repr(0, true);
    if (!representation)
      return std::unexpected(std::move(representation.error()));
    if (auto details = root().logger().debug("Object placement parameters: {}", *representation); !details)
      return std::unexpected(std::move(details.error()));
  }
  return true;
}

std::expected<bool, StarException> ObjectItem::canPlace(bool) const {
  if (initialized()) {
    auto itemOwner = owner();
    if (!itemOwner)
      return std::unexpected(std::move(itemOwner.error()));
    bool inRange = itemOwner->get().isAdmin();
    if (!inRange) {
      auto range = itemOwner->get().inToolRange();
      if (!range)
        return std::unexpected(std::move(range.error()));
      inRange = *range;
    }
    if (inRange) {
      auto aimPosition = itemOwner->get().aimPosition();
      if (!aimPosition)
        return std::unexpected(std::move(aimPosition.error()));
      auto pos = Vec2I(aimPosition->floor());
      auto database = root().objectDatabase();
      if (!database)
        return std::unexpected(std::move(database.error()));
      auto itemWorld = world();
      if (!itemWorld)
        return std::unexpected(std::move(itemWorld.error()));
      auto name = objectName();
      if (!name)
        return std::unexpected(std::move(name.error()));
      return (*database)->canPlaceObject(itemWorld->get(), pos, *name);
    }
  }
  return false;
}

}
