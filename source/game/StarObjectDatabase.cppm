module;
import std;
import star.format;
import star.json;
import star.id_map;
import star.random;
import star.periodic_function;
import star.ordered_map;
import star.lru_cache;
import star.thread;
import star.time;
import star.ttl_cache;
import star.game_types;
import star.material_types;
import star.data_stream;
import star.bi_map;
import star.color;
import star.list;
import star.rect;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.vector;
import star.strong_typedef;
import star.string;
import star.poly;
import star.casting;

import star.item_descriptor;
import star.animation;
import star.particle;
import star.tile_damage;
import star.damage_types;
import star.status_types;
import star.drawable;
import star.entity_rendering_types;
import star.world_geometry;
import star.damage;
import star.entity;
import star.interaction_types;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;

import star.light_source;

import star.net_compatibility;
import star.byte_array;
namespace Star {

class Root;
class World;
class Image;
class ItemDatabase;
class Object;
class LuaRoot;
class Rebuilder;

struct ObjectExceptionTag {
  static constexpr std::string_view name() { return "ObjectException"; }
};
using ObjectException = StarError<ObjectExceptionTag, StarException>;

struct ObjectOrientation {
  struct Anchor {
    TileLayer layer;
    Vec2I position;
    bool tilled;
    bool soil;
    std::optional<MaterialId> material;
  };

  struct ParticleEmissionEntry {
    float particle_emission_rate;
    float particle_emission_rate_variance;
    // Particle positions are considered relative to image pixels, and are
    // flipped with image flipping
    Particle particle;
    Particle particle_variance;
    bool place_in_spaces;
  };

  // The JSON values that were used to configure this orientation.
  Json config;

  EntityRenderLayer render_layer;
  List<Drawable> image_layers;
  bool flip_images;

  // Offset of image from (0, 0) object position, in tile coordinates
  Vec2F image_position;

  // If an object has frames > 1, then the image name will have the marker
  // "{frame}" replaced with an integer in [0, frames)
  int frames = 1;
  float animation_cycle = 1.0f;

  // Spaces the object occupies.  By default, this is simply the single space
  // at the object position, but can be specified in config as either a list of
  // Vec2I, or by setting a threshold value using "spaceScanning", which will
  // scan the image (frame 1) for non-transparent pixels.
  List<Vec2I> spaces;
  RectI bound_box;

  // Allow an orientation to override the metaboundbox in case you don't want to
  // specify spaces
  std::optional<RectF> meta_bound_box;

  // Anchors of the object to place it in the world
  // For background tiles set in order for the object to
  // remain placed.  Must be within 1 space of the bounding box of spaces.
  // For foreground tiles this cannot logically contain any position
  // also in spaces, as objects cannot overlap with foreground tiles.
  List<Anchor> anchors;

  // if true, only one anchor needs to be valid for the orientation to be valid,
  // otherwise all anchors must be valid
  bool anchor_any;

  std::optional<Direction> direction_affinity;

  // Optional list of material spaces
  List<MaterialSpace> material_spaces;

  // optionally override the default spaces used for interaction
  std::optional<List<Vec2I>> interactive_spaces;

  Vec2F light_position;
  float beam_angle;

  List<ParticleEmissionEntry> particle_emitters;

  std::optional<PolyF> status_effect_area;
  Json touch_damage_config;

  [[nodiscard]] static std::expected<ParticleEmissionEntry, StarException> parse_particle_emitter(Root& root, String const& path, Json const& config);
  [[nodiscard]] std::expected<bool, StarException> placement_valid(World const& world, Vec2I const& position) const;
  [[nodiscard]] std::expected<bool, StarException> anchors_valid(World const& world, Vec2I const& position) const;
};

// TODO: This is used very strangely and inconsistently. We go to all the trouble of populating
// this ObjectConfig structure from the JSON, but then keep around the JSON anyway. In some
// places we access the objectConfig, but in many more we use the object's configValue method
// to access the raw config JSON which means it's inconsistent which parameters can be overridden
// by instance values at various levels. This whole system needs reevaluation.
struct ObjectConfig {
  // Returns the index of the best valid orientation.  If no orientations are
  // valid, returns npos
  [[nodiscard]] static std::expected<std::size_t, StarException> find_valid_orientation(
      std::span<std::shared_ptr<ObjectOrientation> const> orientations, World const& world, Vec2I const& position,
      std::optional<Direction> direction_affinity = {});

  String path;
  // The JSON values that were used to configure this Object
  Json config;

  String name;
  String type;
  String race;
  String category;
  StringList colony_tags;
  StringList scripts;
  StringList animation_scripts;

  unsigned price;
  bool printable;
  bool scannable;

  bool interactive;

  StringMap<Color> light_colors;
  LightType light_type;
  float point_beam;
  float beam_ambience;
  std::optional<PeriodicFunction<float>> light_flickering;

  String sound_effect;
  float sound_effect_range_multiplier;

  List<PersistentStatusEffect> status_effects;
  Json touch_damage_config;

  bool has_object_item;
  bool retain_object_parameters_in_item;

  bool smashable;
  bool smash_on_break;
  bool unbreakable;
  String smash_drop_pool;
  List<List<ItemDescriptor>> smash_drop_options;
  StringList smash_sound_options;
  JsonArray smash_particles;

  String break_drop_pool;
  List<List<ItemDescriptor>> break_drop_options;

  TileDamageParameters tile_damage_parameters;
  float damage_shake_magnitude;
  String damage_material_kind;

  EntityDamageTeam damage_team;

  std::optional<float> minimum_liquid_level;
  std::optional<float> maximum_liquid_level;
  float liquid_check_interval;

  float health;

  Json animation_config;

  List<std::shared_ptr<ObjectOrientation>> orientations;

  // If true, the object will root - it will prevent the blocks it is
  // anchored to from being destroyed directly, and damage from those
  // blocks will be redirected to the object
  bool rooting;

  bool biome_placed;
};

struct ObjectPlacementPreview {
  List<Drawable> drawables;
  bool placeable = false;
};

class ObjectDatabase {
  struct ConstructionKey { explicit ConstructionKey() = default; };

public:
  [[nodiscard]] static std::expected<Json, StarException> parse_touch_damage(Root& root, String const& path, Json const& touch_damage);
  [[nodiscard]] static std::expected<List<std::shared_ptr<ObjectOrientation>>, StarException> parse_orientations(
      Root& root, String const& path, Json const& config_list, Json const& base_config);

  ObjectDatabase(ConstructionKey, Root& root);
  [[nodiscard]] static std::expected<std::shared_ptr<ObjectDatabase>, StarException> create(Root& root);

  void cleanup();

  StringList all_objects() const;
  bool is_object(String const& name) const;

  [[nodiscard]] std::expected<std::shared_ptr<ObjectConfig>, StarException> get_config(String const& object_name) const;
  [[nodiscard]] std::expected<std::shared_ptr<List<std::shared_ptr<ObjectOrientation>> const>, StarException> get_orientations(String const& object_name) const;

  [[nodiscard]] std::expected<std::shared_ptr<Object>, StarException> create_object(String const& object_name, Json const& object_parameters = JsonObject()) const;
  [[nodiscard]] std::expected<std::shared_ptr<Object>, StarException> disk_load_object(Json const& disk_store) const;
  [[nodiscard]] std::expected<std::shared_ptr<Object>, StarException> net_load_object(ByteArray const& net_store, NetCompatibilityRules rules = {}) const;

  [[nodiscard]] std::expected<bool, StarException> can_place_object(World const& world, Vec2I const& position,
      String const& object_name, Json const& parameters = {}, std::optional<Direction> direction_affinity = {}) const;
  // If the object is placeable in the given position, creates the given object
  // and sets its position and direction and returns it, otherwise returns
  // null.
  [[nodiscard]] std::expected<std::shared_ptr<Object>, StarException> create_for_placement(World const& world, String const& object_name, Vec2I const& position,
      Direction direction, Json const& parameters = JsonObject()) const;

  // An explicit placement image does not need a placement query when the tool is out of range.
  [[nodiscard]] std::expected<ObjectPlacementPreview, StarException> cursor_hint_drawables(World const& world,
      String const& object_name, Vec2I const& position, Direction direction, Json const& parameters = {},
      bool test_placement = true) const;

private:
  friend class Object;

  struct ConfigLoad {
    std::condition_variable completed;
    bool finished = false;
  };

  [[nodiscard]] static std::expected<List<std::shared_ptr<ObjectOrientation>>, StarException> parse_orientations_impl(
      Root& root, String const& path, Json const& config_list, Json const& base_config, bool load_emitters);
  [[nodiscard]] static std::expected<std::optional<List<std::shared_ptr<ObjectOrientation>>>, StarException>
  parse_custom_orientations(Root& root, ObjectConfig const& config, Json const& parameters, bool load_emitters);
  [[nodiscard]] std::expected<std::shared_ptr<Object>, StarException> construct_object(
      std::shared_ptr<ObjectConfig const> const& config, Json const& parameters) const;
  [[nodiscard]] std::expected<std::shared_ptr<ObjectConfig>, StarException> read_config(String const& path) const;

  Root& m_root;

  StringMap<String> m_paths;
  mutable std::mutex m_cache_mutex;
  mutable HashTtlCache<String, std::shared_ptr<ObjectConfig>> m_config_cache;
  mutable StringMap<std::shared_ptr<ConfigLoad>> m_config_loads;

  std::shared_ptr<Rebuilder> m_rebuilder;
};

}

export module star.object_database;

export namespace Star {
  using ::Star::ObjectExceptionTag;
  using ::Star::ObjectException;
  using ::Star::World;
  using ::Star::Image;
  using ::Star::ItemDatabase;
  using ::Star::Object;
  using ::Star::ObjectOrientation;
  using ::Star::ObjectConfig;
  using ::Star::ObjectDatabase;
  using ::Star::ObjectPlacementPreview;
  using ::Star::LuaRoot;
  using ::Star::Rebuilder;
}
