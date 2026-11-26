module;
import std;
import star.type_identity;
import star.string;
import star.format;
import star.json;
import star.id_map;
import star.set;
import star.casting;
import star.data_stream;
import star.game_types;
import star.strong_typedef;
import star.list;
import star.vector;
import star.bi_map;
import star.rect;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.interaction_types;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_damage;
import star.inspectable_entity;

import star.net_element_basic_fields;
import star.net_element_float_fields;
import star.net_element_system;

import star.net_compatibility;
import star.byte_array;
import star.particle;
namespace Star {
class Root;
class AudioInstance;

class RenderCallback;
class PlantDrop;
struct TreeVariant;
struct GrassVariant;
struct BushVariant;

struct PlantExceptionTag {
  static constexpr std::string_view name() { return "PlantException"; }
};
using PlantException = StarError<PlantExceptionTag, StarException>;

class Plant : public virtual TileEntity {
public:
  void query_type(TypeQuery& query) const noexcept override { query_type_of<Plant, TileEntity>(*this, query); }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<Plant>(); }

  static constexpr float plant_scan_threshold = 0.1f;

  enum RotationType {
    DontRotate,
    RotateBranch,
    RotateLeaves,
    RotateCrownBranch,
    RotateCrownLeaves
  };

  static std::expected<EnumMap<RotationType>, StarException> const rotation_type_names;

  enum PlantPieceKind {
    None,
    Stem,
    Foliage
  };

  struct PlantPiece {
    AssetPath image_path;
    String image;
    Vec2U image_size;
    Vec2F offset;
    int segment_idx = 0;
    bool structural_segment = false;
    PlantPieceKind kind = None;
    RotationType rotation_type = DontRotate;
    float rotation_offset = 0.0f;
    Set<Vec2I> spaces;
    bool flip = false;
    // Draw order is not serialized.
    float z_level = 0.0f;
  };

  [[nodiscard]] static std::expected<std::shared_ptr<Plant>, StarException> create(Root& root, TreeVariant const& config, std::uint64_t seed);
  [[nodiscard]] static std::expected<std::shared_ptr<Plant>, StarException> create(Root& root, GrassVariant const& config, std::uint64_t seed);
  [[nodiscard]] static std::expected<std::shared_ptr<Plant>, StarException> create(Root& root, BushVariant const& config, std::uint64_t seed);
  [[nodiscard]] static std::expected<std::shared_ptr<Plant>, StarException> create(Root& root, Json const& disk_store);
  [[nodiscard]] static std::expected<std::shared_ptr<Plant>, StarException> create(Root& root, ByteArray const& net_store, NetCompatibilityRules rules = {});

  [[nodiscard]] std::expected<Json, StarException> disk_store() const;
  [[nodiscard]] std::expected<ByteArray, StarException> net_store(NetCompatibilityRules rules = {}) const;

  EntityType entity_type() const override;

  [[nodiscard]] std::expected<void, StarException> init(World& world, EntityId entity_id, EntityMode mode) override;

  [[nodiscard]] std::expected<String, StarException> description() const override;

  [[nodiscard]] std::expected<std::pair<ByteArray, std::uint64_t>, StarException> write_net_state(std::uint64_t from_version = 0, NetCompatibilityRules rules = {}) override;
  [[nodiscard]] std::expected<void, StarException> read_net_state(ByteArray const& data, float interpolation_time = 0.0f, NetCompatibilityRules rules = {}) override;

  [[nodiscard]] std::expected<void, StarException> enable_interpolation(float extrapolation_hint) override;
  [[nodiscard]] std::expected<void, StarException> disable_interpolation() override;

  Vec2F position() const override;
  RectF meta_bound_box() const override;

  bool ephemeral() const override;

  [[nodiscard]] std::expected<bool, StarException> should_destroy() const override;

  // Performs the world occupancy checks that should_destroy() avoids.
  [[nodiscard]] std::expected<bool, StarException> check_broken() override;

  // Base tile grid position
  Vec2I tile_position() const override;
  [[nodiscard]] std::expected<void, StarException> set_tile_position(Vec2I const& tile_position) override;

  // Spaces this plant currently occupies
  List<Vec2I> spaces() const override;

  // Root blocks for this plant.
  List<Vec2I> roots() const override;

  [[nodiscard]] std::expected<void, StarException> update(float dt, std::uint64_t current_step) override;

  [[nodiscard]] std::expected<void, StarException> collect_render_events(RenderCallback& render_callback) override;
  bool has_pending_render_events() const;

  [[nodiscard]] std::expected<void, StarException> render(RenderCallback& render_callback) override;

  [[nodiscard]] std::expected<bool, StarException> damage_tiles(List<Vec2I> const& positions, Vec2F const& source_position, TileDamage const& tile_damage) override;

  // Central root position
  Vec2I primary_root() const;
  // Plant hangs from the ceiling
  bool ceiling() const;

  List<PlantPiece> pieces() const;
  RectF interactive_bound_box() const override;

private:
  [[nodiscard]] static std::expected<std::shared_ptr<Plant>, StarException> create_uninitialized(Root& root);
  [[nodiscard]] std::expected<void, StarException> initialize(ByteArray const& net_store, NetCompatibilityRules rules);
  [[nodiscard]] std::expected<void, StarException> initialize(Json const& disk_store);
  [[nodiscard]] std::expected<void, StarException> initialize(BushVariant const& config, std::uint64_t seed);
  [[nodiscard]] std::expected<void, StarException> initialize(GrassVariant const& config, std::uint64_t seed);
  [[nodiscard]] std::expected<void, StarException> initialize(TreeVariant const& config, std::uint64_t seed);

  explicit Plant(Root& root);

  struct PendingFall {
    static constexpr auto no_slot = std::numeric_limits<std::size_t>::max();

    Vec2F break_point;
    Vec2F world_break_point;
    Vec2F fall_vector;
    String description;
    float random;
    bool first = true;
    // Standing piece index <-> stable slot in the flattened segment groups.
    std::vector<std::size_t> slots;
    std::vector<std::size_t> indices;
    std::vector<std::size_t> segment_ends;
    std::shared_ptr<PlantDrop> unpublished_drop;
  };

  [[nodiscard]] std::expected<void, StarException> break_at_position(World& world, Vec2I const& position, Vec2F const& source_position);
  [[nodiscard]] std::expected<void, StarException> publish_fall(World& world, PendingFall& pending);
  [[nodiscard]] std::expected<void, StarException> resume_felling(World& world);
  Vec2I base_damage_position(List<Vec2I> const& positions, WorldGeometry const& geometry) const;

  struct PieceGeometry {
    List<Vec2I> spaces;
    List<Vec2I> roots;
    RectF bound_box;
    RectF meta_bound_box;
  };

  [[nodiscard]] std::expected<PieceGeometry, StarException> prepare_piece_geometry(List<PlantPiece>& pieces) const;
  [[nodiscard]] std::expected<void, StarException> initialize_piece_geometry();
  [[nodiscard]] std::expected<void, StarException> replace_pieces(List<PlantPiece>&& pieces);
  PieceGeometry derive_piece_geometry(List<PlantPiece> const& pieces) const;
  void commit_piece_geometry(PieceGeometry&& geometry);
  float m_previous_wind_time = 0.0f;
  float m_previous_wind_level = 0.0f;
  float m_presentation_damage_x_offset = 0.0f;

  [[nodiscard]] std::expected<void, StarException> read_pieces(ByteArray const& pieces);
  [[nodiscard]] std::expected<ByteArray, StarException> write_pieces() const;

  [[nodiscard]] std::expected<void, StarException> read_pieces_from_json(Json const& pieces);
  [[nodiscard]] std::expected<Json, StarException> write_pieces_to_json() const;

  [[nodiscard]] std::expected<void, StarException> setup_net_states();
  [[nodiscard]] std::expected<void, StarException> load_net_states();
  [[nodiscard]] std::expected<void, StarException> store_net_states();

  Vec2I m_tile_position;
  PieceGeometry m_piece_geometry;

  Json m_descriptions;

  bool m_ephemeral = false;

  Json m_stem_drop_config;
  Json m_foliage_drop_config;
  Json m_sapling_drop_config;

  List<PlantPiece> m_pieces;
  std::unique_ptr<PendingFall> m_pending_fall;
  bool m_pieces_updated = true;
  bool m_pieces_update_pending = false;

  bool m_ceiling = false;
  bool m_broken = false;
  bool m_falls_when_dead = false;

  float m_wind_time = 0.0f;
  float m_wind_level = 0.0f;

  TileDamageParameters m_tile_damage_parameters;
  std::shared_ptr<EntityTileDamageStatus> m_tile_damage_status;
  float m_tile_damage_x = 0.0f;
  float m_tile_damage_y = 0.0f;
  bool m_tile_damage_event_trigger = false;
  bool m_tile_damage_event = false;

  struct DamageEffect {
    Vec2F origin;
    Vec2F offset;
    bool particles_prepared = false;
    std::uint32_t remaining = 0;
    float hue_shift = 0.0f;
    std::shared_ptr<JsonArray const> particle_options;
    std::optional<Json> selected_particle;
    std::optional<Particle> pending_particle;
    std::optional<Json> selected_sound;
    Vec2F sound_position;
    std::shared_ptr<AudioInstance> pending_audio;
  };
  [[nodiscard]] std::expected<bool, StarException> submit_damage_effect(RenderCallback& callback, DamageEffect& effect);
  std::unique_ptr<DamageEffect> m_damage_effect;

  NetElementTopGroup m_net_group;
  NetElementBytes m_pieces_net_state;
  NetElementFloat m_tile_damage_x_net_state;
  NetElementFloat m_tile_damage_y_net_state;
  NetElementEvent m_tile_damage_event_net_state;
};

}

export module star.plant;

export namespace Star {
  using ::Star::RenderCallback;
  using ::Star::Plant;
  using ::Star::TreeVariant;
  using ::Star::GrassVariant;
  using ::Star::BushVariant;
  using ::Star::PlantExceptionTag;
  using ::Star::PlantException;
}
