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
import star.algorithm;

export module star.object_item;
import star.object_database;


export namespace Star {

class ObjectItem : public Item, public FireableItem, public BeamItem {
  struct ConstructionKey { explicit ConstructionKey() = default; };

public:
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<ObjectItem, Item, FireableItem, BeamItem>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<ObjectItem>(); }

  ObjectItem(ConstructionKey, Root& root) : Item(root), BeamItem(root) {}
  [[nodiscard]]
  static std::expected<std::shared_ptr<ObjectItem>, StarException> create(Root& root, Json const& config, String const& directory, Json const& object_parameters);
  ~ObjectItem() override = default;

  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> clone() const override;

  [[nodiscard]]
  std::expected<void, StarException> init(ToolUserEntity& owner, ToolHand hand) override;
  [[nodiscard]]
  std::expected<void, StarException> update(float dt, FireMode fire_mode, bool shifting, HashSet<MoveControlType> const& moves) override;

  [[nodiscard]]
  std::expected<bool, StarException> beam_can_place() const override;

  float cooldown_time() const override;
  [[nodiscard]]
  std::expected<void, StarException> fire(FireMode mode, bool shifting, bool edge_triggered) override;

  [[nodiscard]]
  std::expected<String, StarException> object_name() const;
  [[nodiscard]]
  std::expected<Json, StarException> object_parameters() const;

  [[nodiscard]]
  std::expected<bool, StarException> place_in_world(FireMode mode, bool shifting);
  [[nodiscard]]
  std::expected<bool, StarException> can_place(bool shifting) const;

private:
  [[nodiscard]] std::expected<bool, StarException> placement_in_range() const;

  bool m_shifting;
};

}

namespace Star {

std::expected<std::shared_ptr<ObjectItem>, StarException> ObjectItem::create(Root& root, Json const& config, String const& directory, Json const& object_parameters) {
  auto item = std::make_shared<ObjectItem>(ConstructionKey{}, root);
  auto initialized = item->initialize(config, directory, object_parameters);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto fireable = item->initializeFireable(config);
  if (!fireable)
    return std::unexpected(std::move(fireable.error()));
  auto beam = item->initializeBeam(config);
  if (!beam)
    return std::unexpected(std::move(beam.error()));
  auto two_handed = config.get_bool("twoHanded", true);
  if (!two_handed)
    return std::unexpected(std::move(two_handed.error()));
  item->set_two_handed(*two_handed);
  // Retained script storage participates in the existing item stacking contract.
  auto retain = item->instance_value("retainObjectParametersInItem", false).and_then([](Json const& value) { return value.to_bool(); });
  if (!retain)
    return std::unexpected(std::move(retain.error()));
  if (*retain) {
    auto storage = item->instance_value("scriptStorage");
    if (!storage)
      return std::unexpected(std::move(storage.error()));
    if (storage->is_null()) {
      auto stored = item->set_instance_value("scriptStorage", JsonObject());
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

std::expected<void, StarException> ObjectItem::update(float dt, FireMode fire_mode, bool shifting, HashSet<MoveControlType> const& moves) {
  auto fireable = FireableItem::update(dt, fire_mode, shifting, moves);
  if (!fireable)
    return std::unexpected(std::move(fireable.error()));
  setEnd(BeamItem::EndType::Object);
  m_shifting = shifting;
  auto beam = BeamItem::update(dt, fire_mode, shifting, moves);
  if (!beam)
    return std::unexpected(std::move(beam.error()));
  return {};
}

std::expected<bool, StarException> ObjectItem::beam_can_place() const {
  return can_place(m_shifting);
}

float ObjectItem::cooldown_time() const {
  // TODO: Hardcoded
  return 0.25f;
}

std::expected<void, StarException> ObjectItem::fire(FireMode mode, bool shifting, bool edge_triggered) {
  if (!ready())
    return {};
  auto placed = place_in_world(mode, shifting);
  if (!placed)
    return std::unexpected(std::move(placed.error()));
  if (*placed)
    return FireableItem::fire(mode, shifting, edge_triggered);
  return {};
}

std::expected<String, StarException> ObjectItem::object_name() const {
  return instance_value("objectName", "<objectName missing>").and_then([](Json const& value) { return value.to_string(); });
}

std::expected<Json, StarException> ObjectItem::object_parameters() const {
  Json object_parameters = parameters().opt().value_or(JsonObject{});
  if (!initialized())
    return object_parameters;
  auto item_owner = owner();
  if (!item_owner)
    return std::unexpected(std::move(item_owner.error()));
  return object_parameters.set("owner", json_from_optional(item_owner->get().uniqueId()));
}

std::expected<bool, StarException> ObjectItem::place_in_world(FireMode, bool) {
  if (!initialized())
    return std::unexpected(ItemException("ObjectItem not init'd properly, or user not recognized as Tool User."));
  if (!ready() || empty())
    return false;
  auto in_range = placement_in_range();
  if (!in_range)
    return std::unexpected(std::move(in_range.error()));
  if (!*in_range)
    return false;
  auto item_owner = owner();
  if (!item_owner)
    return std::unexpected(std::move(item_owner.error()));
  auto item_world = world();
  if (!item_world)
    return std::unexpected(std::move(item_world.error()));
  auto aim_position = item_owner->get().aim_position();
  if (!aim_position)
    return std::unexpected(std::move(aim_position.error()));
  auto database = root().object_database();
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto name = object_name();
  if (!name)
    return std::unexpected(std::move(name.error()));
  auto parameters = object_parameters();
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));

  auto attempted = [&]() -> std::expected<bool, StarException> {
    auto object = (*database)->create_for_placement(item_world->get(), *name, Vec2I(aim_position->floor()),
        item_owner->get().walking_direction(), *parameters);
    if (!object)
      return std::unexpected(std::move(object.error()));
    if (!*object)
      return false;
    auto const original_count = count();
    if (!consume(1))
      return false;

    // World insertion may initialize scripts. Keep the reserved unit only
    // after the insertion commits.
    auto restore_count = make_scope_exit([&] { set_count(original_count, true); });
    auto added = item_world->get().addEntity(*object);
    if (!added)
      return std::unexpected(std::move(added.error()));
    restore_count.release();
    return true;
  }();
  if (attempted)
    return *attempted;
  auto logged = root().logger().error_exception(Star::format("Failed to place object '{}'", *name),
                                                ExceptionDiagnostic(attempted.error()));
  if (!logged)
    return std::unexpected(std::move(logged.error()));
  if (root().logger().loggable(LogLevel::Debug)) {
    auto representation = parameters->repr(0, true);
    if (!representation)
      return std::unexpected(std::move(representation.error()));
    if (auto details = root().logger().debug("Object placement parameters: {}", *representation); !details)
      return std::unexpected(std::move(details.error()));
  }
  return false;
}

std::expected<bool, StarException> ObjectItem::placement_in_range() const {
  if (!initialized())
    return false;
  auto item_owner = owner();
  if (!item_owner)
    return std::unexpected(std::move(item_owner.error()));
  if (item_owner->get().is_admin())
    return true;
  return item_owner->get().in_tool_range();
}

std::expected<bool, StarException> ObjectItem::can_place(bool) const {
  auto in_range = placement_in_range();
  if (!in_range || !*in_range)
    return in_range;
  auto item_owner = owner();
  if (!item_owner)
    return std::unexpected(std::move(item_owner.error()));
  auto aim_position = item_owner->get().aim_position();
  if (!aim_position)
    return std::unexpected(std::move(aim_position.error()));
  auto database = root().object_database();
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto item_world = world();
  if (!item_world)
    return std::unexpected(std::move(item_world.error()));
  auto name = object_name();
  if (!name)
    return std::unexpected(std::move(name.error()));
  auto parameters = object_parameters();
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  return (*database)->can_place_object(item_world->get(), Vec2I(aim_position->floor()), *name, *parameters,
      item_owner->get().walking_direction());
}

}
