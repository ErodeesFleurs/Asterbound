module;
import std;
import star.format;
import star.type_identity;
import star.json;
import star.game_types;
import star.casting;
import star.vector;
import star.rect;
import star.list;
import star.rpc_promise;
import star.data_stream;
import star.bi_map;
import star.lua;
import star.string;
import star.lua_components;
import star.entity;
import star.world;
import star.movement_controller;
import star.item_descriptor;
import star.mobile_entity;
import star.scripted_entity;
import star.drawable;
import star.game_timers;

import star.net_element_basic_fields;
import star.net_element_system;

import star.net_compatibility;
import star.byte_array;
namespace Star {
class Root;

class Item;

class ItemDrop : public virtual MobileEntity, public virtual ScriptedEntity {
  struct ConstructionKey { explicit ConstructionKey() = default; };

public:
  ItemDrop(ConstructionKey, Root& root_context);

  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<ItemDrop, MobileEntity, ScriptedEntity>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<ItemDrop>(); }

  // Randomizes the creation position and initial velocity using itemdrop.config.
  [[nodiscard]]
  static std::expected<std::shared_ptr<ItemDrop>, StarException> create_randomized_drop(Root& root_context, std::shared_ptr<Item> const& item, Vec2F const& position, bool eternal = false);
  [[nodiscard]]
  static std::expected<std::shared_ptr<ItemDrop>, StarException> create_randomized_drop(Root& root_context, ItemDescriptor const& item_descriptor, Vec2F const& position, bool eternal = false);

  // Throw speed and the delay before pickup come from itemdrop.config.
  [[nodiscard]]
  static std::expected<std::shared_ptr<ItemDrop>, StarException> throw_drop(Root& root_context, std::shared_ptr<Item> const& item, Vec2F const& position, Vec2F const& velocity, Vec2F const& direction, bool eternal = false);
  [[nodiscard]]
  static std::expected<std::shared_ptr<ItemDrop>, StarException> throw_drop(Root& root_context, ItemDescriptor const& item_descriptor, Vec2F const& position, Vec2F const& velocity, Vec2F const& direction, bool eternal = false);

  [[nodiscard]]
  static std::expected<std::shared_ptr<ItemDrop>, StarException> create(Root& root_context, std::shared_ptr<Item> item);
  [[nodiscard]]
  static std::expected<std::shared_ptr<ItemDrop>, StarException> create(Root& root_context, Json const& disk_store);
  [[nodiscard]]
  static std::expected<std::shared_ptr<ItemDrop>, StarException> create(Root& root_context, ByteArray net_store, NetCompatibilityRules rules = {});

  [[nodiscard]]
  std::expected<Json, StarException> disk_store() const;
  [[nodiscard]]
  std::expected<ByteArray, StarException> net_store(NetCompatibilityRules rules = {}) const;

  EntityType entity_type() const override;

  [[nodiscard]]
  std::expected<void, StarException> init(World& world, EntityId entity_id, EntityMode mode) override;
  [[nodiscard]]
  std::expected<void, StarException> uninit() override;

  [[nodiscard]]
  std::expected<String, StarException> name() const override;
  [[nodiscard]]
  std::expected<String, StarException> description() const override;

  [[nodiscard]]
  std::expected<std::pair<ByteArray, std::uint64_t>, StarException> write_net_state(std::uint64_t from_version = 0, NetCompatibilityRules rules = {}) override;
  [[nodiscard]]
  std::expected<void, StarException> read_net_state(ByteArray data, float interpolation_time = 0.0f, NetCompatibilityRules rules = {}) override;

  [[nodiscard]]
  std::expected<void, StarException> enable_interpolation(float extrapolation_hint = 0.0f) override;
  [[nodiscard]]
  std::expected<void, StarException> disable_interpolation() override;

  Vec2F position() const override;
  RectF meta_bound_box() const override;

  bool ephemeral() const override;

  RectF collision_area() const override;

  [[nodiscard]]
  std::expected<void, StarException> update(float dt, std::uint64_t current_step) override;

  [[nodiscard]]
  std::expected<bool, StarException> should_destroy() const override;

  std::uint64_t presentation_epoch() const override;

  [[nodiscard]]
  std::expected<void, StarException> render(RenderCallback& render_callback) override;
  [[nodiscard]]
  std::expected<void, StarException> render_light_sources(RenderCallback& render_callback) override;
  
  MovementController& movement_controller() override;
  
  // The item that this drop contains
  std::shared_ptr<Item> item() const;

  void set_eternal(bool eternal);

  // Delay pickup by the given amount of game time.
  void set_intangible_time(float intangible_time);

  // Mark this drop as taken by the given entity.  The drop will animate
  // towards them for a while and then disappear.
  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> take_by(EntityId entity_id, float time_offset = 0.0f);

  // Mark this drop as taken, but do not animate it towards a player simply
  // disappear next step.
  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> take();

  // Native preflight does not run scripts; the final pickup checks Lua permission.
  bool pickup_available() const;
  [[nodiscard]]
  std::expected<bool, StarException> can_take() const;

  void set_position(Vec2F const& position);

  Vec2F velocity() const;
  void set_velocity(Vec2F const& velocity);
  
  [[nodiscard]]
  std::expected<Json, StarException> config_value(String const& name, Json const& def = Json()) const;

  [[nodiscard]]
  std::expected<std::optional<LuaValue>, StarException> call_script(String const& func, LuaVariadic<LuaValue> const& args) override;
  [[nodiscard]]
  std::expected<std::optional<LuaValue>, StarException> eval_script(String const& code) override;
  
  ClientEntityMode client_entity_mode() const override;

private:
  enum class Mode { Intangible, Available, Taken, Dead };
  static std::expected<EnumMap<Mode>, StarException> const mode_names;

  [[nodiscard]]
  std::expected<void, StarException> initialize();
  [[nodiscard]]
  std::expected<void, StarException> initialize_item(std::shared_ptr<Item> item);
  void replace_item(std::shared_ptr<Item> replacement);
  [[nodiscard]]
  std::expected<void, StarException> update_taken(bool master);

  [[nodiscard]]
  std::expected<LuaCallbacks, StarException> make_item_drop_callbacks();

  Json m_config;
  Json m_parameters;
  std::shared_ptr<Item> m_item;
  RectF m_bound_box;
  float m_after_taken_life;
  float m_overhead_time;
  float m_pickup_distance;
  float m_velocity;
  float m_velocity_approach;
  float m_overhead_approach;
  Vec2F m_overhead_offset;

  float m_combine_chance;
  float m_combine_radius;
  double m_age_items_every;

  NetElementTopGroup m_net_group;
  NetElementEnum<Mode> m_mode;
  NetElementIntegral<EntityId> m_owning_entity;
  NetElementData<ItemDescriptor> m_item_descriptor;
  bool m_item_descriptor_pending = false;
  std::shared_ptr<MovementController> m_movement_controller;

  // Only updated on master
  bool m_eternal;
  EpochTimer m_drop_age;
  GameTimer m_intangible_timer;
  EpochTimer m_age_items_timer;

  bool m_draw_rarity_beam;
  bool m_over_foreground;
  std::optional<List<Drawable>> m_drawables;
  
  ClientEntityMode m_client_entity_mode;
  
  mutable LuaEntityScript m_script_component;
  mutable bool m_checking_pickup = false;
  std::optional<Mode> m_override_mode;
};

}

export module star.item_drop;

export namespace Star {
  using ::Star::Item;
  using ::Star::ItemDrop;
}
