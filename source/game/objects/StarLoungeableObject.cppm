module;
import star.format;
import std;
import star.type_identity;
import star.json;
import star.json_extra;
import star.id_map;
import star.string;
import star.data_stream;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.casting;
import star.vector;
import star.game_types;
import star.strong_typedef;
import star.bi_map;
import star.random;
import star.periodic;
import star.periodic_function;
import star.set;
import star.lua;
import star.audio;
import star.map;
import star.ordered_map;


import star.lua_root;
import star.lua_components;
import star.lua_animation_component;
import star.tile_damage;
import star.interaction_types;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.status_effect_entity;
import star.scripted_entity;
import star.chat_action;
import star.chatty_entity;
import star.wiring;
import star.wire_entity;
import star.inspectable_entity;
import star.animated_part_set;
import star.animation;
import star.particle;
import star.mixer;
import star.networked_animator;
import star.entity_rendering;
import star.object;
import star.drawable;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.anchorable_entity;
import star.entity_rendering_types;
import star.lounging_entities;


namespace Star {
class Root;

class LoungeableObject : public Object, public virtual LoungeableEntity {
public:
  void const* query_type(TypeKey key) const noexcept override {
    return query_type_of<LoungeableObject, Object, LoungeableEntity>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<LoungeableObject>(); }

  [[nodiscard]] static std::expected<std::shared_ptr<LoungeableObject>, StarException> create(Root& rootContext, std::shared_ptr<ObjectConfig const> config, Json const& parameters = Json());

  [[nodiscard]] std::expected<void, StarException> render(RenderCallback& renderCallback) override;

  [[nodiscard]] std::expected<InteractAction, StarException> interact(InteractRequest const& request) override;

  std::size_t anchorCount() const override;
  [[nodiscard]] std::expected<std::shared_ptr<LoungeAnchor const>, StarException> loungeAnchor(std::size_t positionIndex) const override;

protected:
  [[nodiscard]] std::expected<void, StarException> setOrientationIndex(std::size_t orientationIndex) override;

private:
  explicit LoungeableObject(Root& rootContext);
  List<Vec2F> m_sitPositions;
  bool m_sitFlipDirection;
  LoungeOrientation m_sitOrientation;
  float m_sitAngle;
  String m_sitCoverImage;
  bool m_flipImages;
  List<PersistentStatusEffect> m_sitStatusEffects;
  StringSet m_sitEffectEmitters;
  std::optional<String> m_sitEmote;
  std::optional<String> m_sitDance;
  JsonObject m_sitArmorCosmeticOverrides;
  std::optional<String> m_sitCursorOverride;
};

}

export module star.loungeable_object;

export namespace Star {
  using ::Star::LoungeableObject;
}
