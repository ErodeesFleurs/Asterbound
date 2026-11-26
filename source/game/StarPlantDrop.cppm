module;
import std;
import star.asset_path;
import star.byte_array;
import star.entity;
import star.format;
import star.game_types;
import star.json;
import star.list;
import star.mobile_entity;
import star.movement_controller;
import star.net_compatibility;
import star.net_element_basic_fields;
import star.net_element_system;
import star.particle;
import star.plant;
import star.rect;
import star.set;
import star.string;
import star.type_identity;
import star.vector;

namespace Star {
class Root;

class RenderCallback;
class AudioInstance;
class ItemDrop;

class PlantDrop : public virtual MobileEntity {
public:
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<PlantDrop, MobileEntity>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<PlantDrop>(); }

  [[nodiscard]] static std::expected<std::shared_ptr<PlantDrop>, StarException> create(Root& root, List<Plant::PlantPiece> pieces,
      Vec2F const& position, Vec2F const& strike_vector, String const& description, bool upside_down,
      Json stem_config, Json foliage_config, Json sapling_config, bool master, float random);
  [[nodiscard]] static std::expected<std::shared_ptr<PlantDrop>, StarException> create(Root& root, ByteArray const& net_store, NetCompatibilityRules rules = {});

  [[nodiscard]] std::expected<ByteArray, StarException> net_store(NetCompatibilityRules rules = {});

  EntityType entity_type() const override;

  [[nodiscard]] std::expected<void, StarException> init(World& world, EntityId entity_id, EntityMode mode) override;
  [[nodiscard]] std::expected<void, StarException> uninit() override;

  [[nodiscard]] std::expected<String, StarException> description() const override;

  [[nodiscard]] std::expected<std::pair<ByteArray, std::uint64_t>, StarException> write_net_state(std::uint64_t from_version = 0, NetCompatibilityRules rules = {}) override;
  [[nodiscard]] std::expected<std::pair<ByteArray, std::uint64_t>, StarException> snapshot_net_state(NetCompatibilityRules rules = {}) const;
  [[nodiscard]] std::expected<void, StarException> read_net_state(ByteArray const& data, float interpolation_time = 0.0f, NetCompatibilityRules rules = {}) override;

  [[nodiscard]] std::expected<void, StarException> enable_interpolation(float extrapolation_hint = 0.0f) override;
  [[nodiscard]] std::expected<void, StarException> disable_interpolation() override;

  Vec2F position() const override;
  RectF meta_bound_box() const override;

  [[nodiscard]] std::expected<bool, StarException> should_destroy() const override;

  [[nodiscard]] std::expected<void, StarException> update(float dt, std::uint64_t current_step) override;

  std::uint64_t presentation_epoch() const override;

  [[nodiscard]] std::expected<void, StarException> collect_render_events(RenderCallback& render_callback) override;
  bool has_pending_render_events() const;

  [[nodiscard]] std::expected<void, StarException> render(RenderCallback& render_callback) override;

  MovementController& movement_controller() override;

private:
  friend class Plant;

  [[nodiscard]] static std::expected<std::shared_ptr<PlantDrop>, StarException> create_segment(Root& root,
      List<Plant::PlantPiece> const& pieces, std::span<std::size_t const> indices, Vec2F const& break_point,
      Vec2F const& position, Vec2F const& strike_vector, String const& description, bool upside_down,
      Json stem_config, Json foliage_config, Json sapling_config, bool master, float random);

  template <bool prepared_paths, std::ranges::sized_range Pieces>
    requires std::ranges::input_range<Pieces>
      && std::same_as<std::ranges::range_reference_t<Pieces>, Plant::PlantPiece const&>
  [[nodiscard]] static std::expected<std::shared_ptr<PlantDrop>, StarException> create_fall(Root& root, Pieces&& pieces,
      Vec2F const& break_point, Vec2F const& position, Vec2F const& strike_vector, String const& description,
      bool upside_down, Json stem_config, Json foliage_config, Json sapling_config, bool master, float random);

  explicit PlantDrop(Root& root);
  [[nodiscard]] std::expected<void, StarException> initialize_net_elements(Root& root);
  [[nodiscard]] std::expected<void, StarException> initialize(ByteArray const& net_store, NetCompatibilityRules rules);

  struct PlantDropPiece {
    AssetPath image;
    Vec2F offset;
    Plant::PlantPieceKind kind = Plant::PlantPieceKind::None;
    bool flip = false;
  };

  struct ItemDelivery {
    Vec2F origin;
    float rotation;
    std::size_t piece_index = 0;
    std::size_t item_index = 0;
    std::optional<Json> selection;
    std::shared_ptr<JsonArray const> items;
    std::optional<Vec2F> center;
    std::optional<Vec2F> position;
    std::shared_ptr<ItemDrop> pending_item;
  };

  struct ParticleBurst {
    std::shared_ptr<JsonArray const> options;
    Vec2F image_size;
    Set<Vec2I> spaces;
    std::uint32_t remaining = 0;
    float hue_shift = 0.0f;
    std::optional<Vec2F> sampled_position;
    std::optional<Json> sampled_config;
    std::optional<Particle> pending_particle;
  };

  struct RenderBurst {
    Vec2F origin;
    float rotation;
    std::size_t sound_index = 0;
    std::size_t piece_index = 0;
    std::optional<Json> selected_sound;
    std::shared_ptr<AudioInstance> pending_audio;
    std::optional<ParticleBurst> particles;
  };

  std::optional<std::reference_wrapper<Json const>> piece_config(Plant::PlantPieceKind kind) const;
  RectF collision_rect(float rotation) const;
  [[nodiscard]] std::expected<void, StarException> deliver_items(World& world, ItemDelivery& delivery);
  [[nodiscard]] std::expected<ParticleBurst, StarException> prepare_particles(PlantDropPiece const& piece, Json const& config, String const& mode);
  [[nodiscard]] std::expected<void, StarException> submit_particles(RenderCallback& callback, RenderBurst const& burst,
      PlantDropPiece const& piece, ParticleBurst& particles, std::size_t& budget);
  [[nodiscard]] std::expected<bool, StarException> submit_render_burst(RenderCallback& callback, RenderBurst& burst,
      String const& mode, std::size_t& budget);
  void observe_replica_effects();

  NetElementTopGroup m_net_group;
  String m_description;
  float m_time = 5000.0f;
  std::shared_ptr<MovementController> m_movement_controller;
  RectF m_collision_rect;
  RectF m_bounding_box;
  float m_rotation_rate = 0.0f;
  // Actual unwrapped angular travel preserves fast authored falling spins.
  float m_presentation_rotation_travel = 0.0f;
  float m_rotation_fall_threshold = 0.0f;
  float m_rotation_cap = 0.0f;
  List<PlantDropPiece> m_pieces;
  Json m_stem_config;
  Json m_foliage_config;
  Json m_sapling_config;
  bool m_master = false; // Serialized: the first segment owns the break sounds.
  NetElementBool m_spawned_drops;
  std::unique_ptr<ItemDelivery> m_item_delivery;
  std::optional<std::pair<Vec2F, float>> m_impact_pose;
  std::unique_ptr<RenderBurst> m_render_burst;
  bool m_observed_live = false;
  bool m_break_pending = false;
  bool m_hit_finished = false;
};

}

export module star.plant_drop;

export namespace Star {
  using ::Star::RenderCallback;
  using ::Star::PlantDrop;
}
