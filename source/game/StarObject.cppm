module;
import star.format;
import star.game_timers;
import star.json_extra;
import std;
import star.type_identity;
import star.json;
import star.id_map;
import star.random;
import star.periodic;
import star.periodic_function;
import star.casting;
import star.strong_typedef;
import star.set;
import star.color;
import star.lua;
import star.vector;
import star.data_stream;
import star.game_types;
import star.message_types;
import star.audio;
import star.thread;
import star.list;
import star.map;
import star.bi_map;
import star.string;
import star.poly;
import star.rect;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ordered_map;

import star.net_element_basic_fields;
import star.net_element_float_fields;
import star.net_element_containers;
import star.net_element_system;

import star.net_compatibility;
import star.byte_array;
namespace Star {
class Root;
class AudioInstance;
class ObjectDatabase;
struct ObjectConfig;
struct ObjectOrientation;
class Object;
}

import star.lua_root;
import star.lua_components;
import star.lua_animation_component;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.entity;
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
import star.drawable;
import star.animation;
import star.particle;
import star.mixer;
import star.light_source;
import star.networked_animator;
import star.damage_types;
import star.entity_rendering_types;
import star.entity_rendering;

namespace Star {


class Object
  : public virtual TileEntity,
    public virtual StatusEffectEntity,
    public virtual ScriptedEntity,
    public virtual ChattyEntity,
    public virtual InspectableEntity,
    public virtual WireEntity {
  struct ConstructionKey { explicit ConstructionKey() = default; };

public:
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<Object, StatusEffectEntity, ScriptedEntity, ChattyEntity, InspectableEntity, WireEntity>(*this,
                                                                                                                  query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<Object>(); }

  Object(ConstructionKey, Root& root);
  [[nodiscard]] static std::expected<std::shared_ptr<Object>, StarException> create(Root& root_context, std::shared_ptr<ObjectConfig const> config, Json const& parameters = JsonObject());
  ~Object() override;

  [[nodiscard]] std::expected<Json, StarException> disk_store() const;
  [[nodiscard]] std::expected<ByteArray, StarException> net_store(NetCompatibilityRules rules = {});

  EntityType entity_type() const override;
  ClientEntityMode client_entity_mode() const override;

  [[nodiscard]] std::expected<void, StarException> init(World& world, EntityId entity_id, EntityMode mode) override;
  [[nodiscard]] std::expected<void, StarException> uninit() override;

  Vec2F position() const override;
  RectF meta_bound_box() const override;

  [[nodiscard]] std::expected<std::pair<ByteArray, std::uint64_t>, StarException>
  write_net_state(std::uint64_t from_version = 0, NetCompatibilityRules rules = {}) override;
  [[nodiscard]] std::expected<void, StarException> read_net_state(ByteArray const& data, float interpolation_time = 0.0f,
                                                                NetCompatibilityRules rules = {}) override;

  [[nodiscard]] std::expected<String, StarException> name() const override;
  [[nodiscard]] std::expected<String, StarException> description() const override;

  bool inspectable() const override;
  [[nodiscard]] std::expected<std::optional<String>, StarException> inspection_log_name() const override;
  [[nodiscard]] std::expected<std::optional<String>, StarException>
  inspection_description(String const& species) const override;

  [[nodiscard]] std::expected<List<LightSource>, StarException> light_sources() const override;

  [[nodiscard]] std::expected<bool, StarException> should_destroy() const override;
  [[nodiscard]] std::expected<void, StarException>
  destroy(std::optional<std::reference_wrapper<RenderCallback>> render_callback) override;

  [[nodiscard]] std::expected<void, StarException> update(float dt, std::uint64_t current_step) override;

  [[nodiscard]] std::expected<void, StarException> collect_render_events(RenderCallback& render_callback) override;

  [[nodiscard]] std::expected<void, StarException> render(RenderCallback& render_callback) override;

  [[nodiscard]] std::expected<void, StarException> render_light_sources(RenderCallback& render_callback) override;

  [[nodiscard]] std::expected<bool, StarException> check_broken() override;

  Vec2I tile_position() const override;

  List<Vec2I> spaces() const override;
  List<MaterialSpace> material_spaces() const override;
  List<Vec2I> roots() const override;

  Direction direction() const;
  void set_direction(Direction direction);

  // Updates tile position and calls updateOrientation
  [[nodiscard]] std::expected<void, StarException> set_tile_position(Vec2I const& pos) override;

  // Find a new valid orientation for the object
  [[nodiscard]] std::expected<void, StarException> update_orientation();
  List<Vec2I> anchor_positions() const;

  [[nodiscard]] virtual std::expected<List<Drawable>, StarException> cursor_hint_drawables() const;

  [[nodiscard]] std::expected<String, StarException> short_description() const;
  String category() const;

  virtual std::shared_ptr<ObjectOrientation> current_orientation() const;

  [[nodiscard]] std::expected<List<PersistentStatusEffect>, StarException> status_effects() const override;
  PolyF status_effect_area() const override;

  [[nodiscard]] std::expected<List<DamageSource>, StarException> damage_sources() const override;

  [[nodiscard]] std::expected<std::optional<HitType>, StarException>
  query_hit(DamageSource const& source) const override;
  [[nodiscard]] std::expected<std::optional<PolyF>, StarException> hit_poly() const override;

  [[nodiscard]] std::expected<List<DamageNotification>, StarException>
  apply_damage(DamageRequest const& damage) override;

  [[nodiscard]] std::expected<bool, StarException> damage_tiles(List<Vec2I> const& position, Vec2F const& source_position,
                                                               TileDamage const& tile_damage) override;
  [[nodiscard]] std::expected<bool, StarException> can_be_damaged() const override;

  RectF interactive_bound_box() const override;

  bool is_interactive() const override;
  [[nodiscard]] std::expected<InteractAction, StarException> interact(InteractRequest const& request) override;
  List<Vec2I> interactive_spaces() const override;

  [[nodiscard]] std::expected<std::optional<LuaValue>, StarException> call_script(String const& func, LuaVariadic<LuaValue> const& args) override;
  [[nodiscard]] std::expected<std::optional<LuaValue>, StarException> eval_script(String const& code) override;

  [[nodiscard]] std::expected<Vec2F, StarException> mouth_position() const override;
  [[nodiscard]] std::expected<Vec2F, StarException> mouth_position(bool ignore_adjustments) const override;
  List<ChatAction> pull_pending_chat_actions() override;

  [[nodiscard]] std::expected<void, StarException> break_object(bool smash = true);

  std::size_t node_count(WireDirection direction) const override;
  [[nodiscard]] std::expected<Vec2I, StarException> node_position(WireNode wire_node) const override;
  [[nodiscard]] std::expected<List<WireConnection>, StarException> connections_for_node(WireNode wire_node) const override;
  [[nodiscard]] std::expected<bool, StarException> node_state(WireNode wire_node) const override;

  [[nodiscard]] std::expected<String, StarException> node_icon(WireNode wire_node) const override;
  [[nodiscard]] std::expected<Color, StarException> node_color(WireNode wire_node) const override;

  [[nodiscard]] std::expected<void, StarException> add_node_connection(WireNode wire_node,
                                                                     WireConnection node_connection) override;
  [[nodiscard]] std::expected<void, StarException> remove_node_connection(WireNode wire_node,
                                                                        WireConnection node_connection) override;

  [[nodiscard]] std::expected<void, StarException> evaluate(WireCoordinator& coordinator) override;

  [[nodiscard]] std::expected<List<QuestArcDescriptor>, StarException> offered_quests() const override;
  [[nodiscard]] std::expected<StringSet, StarException> turn_in_quests() const override;
  [[nodiscard]] std::expected<Vec2F, StarException> quest_indicator_position() const override;

  [[nodiscard]] MessageResponseResult receive_message(ConnectionId sending_connection, String const& message, JsonArray const& args = {}) override;

  // Check, in order, the passed in object parameters, the config parameters,
  // and then the orientation parameters for the given key.  Returns 'def' if
  // no value is found.
  [[nodiscard]] std::expected<Json, StarException> config_value(String const& name, Json const& def = Json()) const;

  std::shared_ptr<ObjectConfig const> config() const;

  [[nodiscard]] std::expected<float, StarException> liquid_fill_level() const;

  bool biome_placed() const;

  using Entity::set_unique_id;

protected:
  friend class ObjectDatabase;
  explicit Object(Root& root_context);
  [[nodiscard]] std::expected<void, StarException> initialize(std::shared_ptr<ObjectConfig const> config, Json const& parameters);

  // Will be automatically called at appropriate times.  Derived classes must
  // call base class versions.
  [[nodiscard]] virtual std::expected<void, StarException> get_net_states(bool initial);
  [[nodiscard]] virtual std::expected<void, StarException> set_net_states();

  [[nodiscard]] virtual std::expected<void, StarException> read_stored_data(Json const& disk_store);
  [[nodiscard]] virtual std::expected<Json, StarException> write_stored_data() const;

  void set_image_key(String const& name, String const& value);

  std::size_t orientation_index() const;
  [[nodiscard]] virtual std::expected<void, StarException> set_orientation_index(std::size_t orientation_index);

  PolyF volume() const;

  LuaStoredEntityScript m_script_component;
  mutable LuaAnimationScript m_scripted_animator;

  NetElementTopGroup m_net_group;
  NetElementBool m_interactive;
  NetElementData<List<MaterialSpace>> m_material_spaces;

private:
  struct Node {
    [[nodiscard]] static std::expected<Node, StarException> create(Json const& position_config, Json const& config, WireDirection direction);
    Vec2I position;
    NetElementData<List<WireConnection>> connections;
    NetElementBool state;
    Color color;
    String icon;
  };
  List<Node>& nodes(WireDirection direction);
  List<Node> const& nodes(WireDirection direction) const;
  [[nodiscard]] std::expected<std::reference_wrapper<Node>, StarException> node(WireNode wire_node);
  [[nodiscard]] std::expected<std::reference_wrapper<Node const>, StarException> node(WireNode wire_node) const;
  [[nodiscard]] std::expected<bool, StarException> node_connected(WireNode wire_node) const;
  [[nodiscard]] std::expected<LuaTable, StarException> node_ids(LuaEngine& engine, WireNode wire_node) const;

  [[nodiscard]] std::expected<LuaCallbacks, StarException> make_object_callbacks();
  [[nodiscard]] std::expected<LuaCallbacks, StarException> make_animator_object_callbacks();

  using DrawableLayers = SmallList<Drawable, 1>;
  [[nodiscard]] std::expected<List<Drawable>, StarException> orientation_drawables(std::size_t orientation_index) const;
  [[nodiscard]] std::expected<std::shared_ptr<DrawableLayers const>, StarException> orientation_layers(std::size_t orientation_index) const;

  [[nodiscard]] std::expected<void, StarException> add_chat_message(String const& message, Json const& config, String const& portrait = "");


  EntityRenderLayer render_layer() const;

  // Lights are sampled for display; emissions are collected once per logic tick.
  void append_configured_light_source(List<LightSource>& lights) const;
  struct RenderEventCollection;
  [[nodiscard]] std::expected<void, StarException> collect_particles(RenderCallback& render_callback, RenderEventCollection& events);
  [[nodiscard]] std::expected<void, StarException> collect_sounds(RenderCallback& render_callback, RenderEventCollection& events);
  std::unique_ptr<RenderEventCollection> m_render_event_collection;

  List<std::shared_ptr<ObjectOrientation>> const& get_orientations() const;

  [[nodiscard]] std::expected<Vec2F, StarException> damage_shake() const;
  [[nodiscard]] std::expected<Vec2F, StarException> compute_damage_shake() const;
  Vec2F m_presentation_damage_shake;

  [[nodiscard]] std::expected<void, StarException> check_liquid_broken();
  GameTimer m_liquid_check_timer;

  std::shared_ptr<ObjectConfig const> m_config;
  std::optional<List<std::shared_ptr<ObjectOrientation>>> m_orientations;
  NetElementHashMap<String, Json> m_parameters;

  NetElementData<std::optional<String>> m_unique_id_net_state;

  NetElementInt m_x_tile_position;
  NetElementInt m_y_tile_position;
  NetElementEnum<Direction> m_direction;
  float m_animation_timer;
  int m_current_frame;

  Directives m_directives;
  Directives m_color_directives;
  String m_color_suffix;

  std::optional<PeriodicFunction<float>> m_light_flickering;

  std::shared_ptr<EntityTileDamageStatus> m_tile_damage_status;

  bool m_broken;
  bool m_unbreakable;
  NetElementFloat m_health;

  std::size_t m_orientation_index;
  NetElementSize m_orientation_index_net_state;
  NetElementHashMap<String, String> m_net_image_keys;
  StringMap<String> m_image_keys;

  void reset_emission_timers();
  // An unrepresentable reciprocal (including an exactly zero sampled rate)
  // retains the legacy infinite cooldown until the orientation resets it.
  List<std::optional<GameTimer>> m_emission_timers;
  std::size_t m_emission_timer_revision = 0;

  NetElementBool m_sound_effect_enabled;
  std::shared_ptr<AudioInstance> m_sound_effect;

  NetElementData<Color> m_light_source_color;

  Vec2F m_animation_position;
  float m_animation_center_line;
  std::shared_ptr<NetworkedAnimator> m_networked_animator;
  NetworkedAnimator::DynamicTarget m_networked_animator_dynamic_target;

  List<ChatAction> m_pending_chat_actions;
  NetElementEvent m_new_chat_message_event;
  NetElementString m_chat_message;
  NetElementString m_chat_portrait;
  NetElementData<Json> m_chat_config;

  mutable std::optional<std::pair<std::size_t, std::shared_ptr<DrawableLayers const>>> m_orientation_drawables_cache;

  List<Node> m_input_nodes;
  List<Node> m_output_nodes;

  NetElementData<List<QuestArcDescriptor>> m_offered_quests;
  NetElementData<StringSet> m_turn_in_quests;

  NetElementHashMap<String, Json> m_scripted_animation_parameters;

  NetElementData<List<DamageSource>> m_damage_sources;
  // Only successfully parsed templates are cached, by effective orientation index.
  mutable HashMap<std::size_t, DamageSource> m_touch_damage_templates;

  ClientEntityMode m_client_entity_mode;
};

}

export module star.object;

export namespace Star {
  using ::Star::AudioInstance;
  using ::Star::ObjectDatabase;
  using ::Star::ObjectConfig;
  using ::Star::ObjectOrientation;
  using ::Star::Object;
}
