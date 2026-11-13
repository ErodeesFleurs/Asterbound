module;
import star.format;
import std;
import star.type_identity;
import star.json;
import star.json_extra;
import star.id_map;
import star.strong_typedef;
import star.data_stream;
import star.vector;
import star.game_types;
import star.casting;
import star.bi_map;
import star.list;
import star.random;
import star.periodic;
import star.periodic_function;
import star.set;
import star.color;
import star.lua;
import star.audio;
import star.thread;
import star.map;
import star.string;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ordered_map;


import star.uuid;
import star.celestial_coordinate;
import star.warping;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.tile_damage;
import star.interaction_types;
import star.item_descriptor;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.warp_target_entity;
import star.lua_components;
import star.lua_animation_component;
import star.status_effect_entity;
import star.scripted_entity;
import star.chat_action;
import star.chatty_entity;
import star.wiring;
import star.wire_entity;
import star.inspectable_entity;
import star.animated_part_set;
import star.drawable;
import star.animation;
import star.particle;
import star.mixer;
import star.networked_animator;
import star.entity_rendering_types;
import star.entity_rendering;
import star.object;
import star.root;

export module star.teleporter_object;

export namespace Star {

class TeleporterObject : public Object, public WarpTargetEntity {
public:
  void const* query_type(TypeKey key) const noexcept override {
    return query_type_of<TeleporterObject, Object, WarpTargetEntity>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<TeleporterObject>(); }

  [[nodiscard]] static std::expected<std::shared_ptr<TeleporterObject>, StarException> create(Root& rootContext, std::shared_ptr<ObjectConfig const> config, Json const& parameters = JsonObject());

  [[nodiscard]] std::expected<Vec2F, StarException> footPosition() const override;
private:
  explicit TeleporterObject(Root& rootContext);
};

}

namespace Star {

TeleporterObject::TeleporterObject(Root& rootContext) : Entity(rootContext), Object(rootContext) {}

std::expected<std::shared_ptr<TeleporterObject>, StarException> TeleporterObject::create(Root& rootContext, std::shared_ptr<ObjectConfig const> config, Json const& parameters) {
  auto object = std::shared_ptr<TeleporterObject>(new TeleporterObject(rootContext));
  auto initialized = object->initialize(std::move(config), parameters);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto idConfig = object->configValue("uniqueId", Uuid(rootContext.random()).hex());
  if (!idConfig)
    return std::unexpected(std::move(idConfig.error()));
  auto id = idConfig->optString();
  if (!id)
    return std::unexpected(std::move(id.error()));
  object->setUniqueId(std::move(*id));
  return object;
}

std::expected<Vec2F, StarException> TeleporterObject::footPosition() const {
  auto foot = configValue("teleporterFootPosition");
  if (!foot)
    return std::unexpected(std::move(foot.error()));
  if (*foot)
    return jsonToVec2F(*foot);
  return Vec2F();
}

}
