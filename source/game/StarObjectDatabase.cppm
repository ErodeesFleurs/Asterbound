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
class RecipeDatabase;
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
    float particleEmissionRate;
    float particleEmissionRateVariance;
    // Particle positions are considered relative to image pixels, and are
    // flipped with image flipping
    Particle particle;
    Particle particleVariance;
    bool placeInSpaces;
  };

  // The JSON values that were used to configure this orientation.
  Json config;

  EntityRenderLayer renderLayer;
  List<Drawable> imageLayers;
  bool flipImages;

  // Offset of image from (0, 0) object position, in tile coordinates
  Vec2F imagePosition;

  // If an object has frames > 1, then the image name will have the marker
  // "{frame}" replaced with an integer in [0, frames)
  unsigned frames;
  float animationCycle;

  // Spaces the object occupies.  By default, this is simply the single space
  // at the object position, but can be specified in config as either a list of
  // Vec2I, or by setting a threshold value using "spaceScanning", which will
  // scan the image (frame 1) for non-transparent pixels.
  List<Vec2I> spaces;
  RectI boundBox;

  // Allow an orientation to override the metaboundbox in case you don't want to
  // specify spaces
  std::optional<RectF> metaBoundBox;

  // Anchors of the object to place it in the world
  // For background tiles set in order for the object to
  // remain placed.  Must be within 1 space of the bounding box of spaces.
  // For foreground tiles this cannot logically contain any position
  // also in spaces, as objects cannot overlap with foreground tiles.
  List<Anchor> anchors;

  // if true, only one anchor needs to be valid for the orientation to be valid,
  // otherwise all anchors must be valid
  bool anchorAny;

  std::optional<Direction> direction_affinity;

  // Optional list of material spaces
  List<MaterialSpace> materialSpaces;

  // optionally override the default spaces used for interaction
  std::optional<List<Vec2I>> interactiveSpaces;

  Vec2F lightPosition;
  float beamAngle;

  List<ParticleEmissionEntry> particleEmitters;

  std::optional<PolyF> statusEffectArea;
  Json touchDamageConfig;

  [[nodiscard]] static std::expected<ParticleEmissionEntry, StarException> parseParticleEmitter(Root& root, String const& path, Json const& config);
  [[nodiscard]] std::expected<bool, StarException> placementValid(World const& world, Vec2I const& position) const;
  [[nodiscard]] std::expected<bool, StarException> anchorsValid(World const& world, Vec2I const& position) const;
};

// TODO: This is used very strangely and inconsistently. We go to all the trouble of populating
// this ObjectConfig structure from the JSON, but then keep around the JSON anyway. In some
// places we access the objectConfig, but in many more we use the object's configValue method
// to access the raw config JSON which means it's inconsistent which parameters can be overridden
// by instance values at various levels. This whole system needs reevaluation.
struct ObjectConfig {
  // Returns the index of the best valid orientation.  If no orientations are
  // valid, returns npos
  [[nodiscard]] std::expected<std::size_t, StarException> find_valid_orientation(World const& world, Vec2I const& position, std::optional<Direction> direction_affinity = std::optional<Direction>()) const;

  String path;
  // The JSON values that were used to configure this Object
  Json config;

  String name;
  String type;
  String race;
  String category;
  StringList colonyTags;
  StringList scripts;
  StringList animationScripts;

  unsigned price;
  bool printable;
  bool scannable;

  bool interactive;

  StringMap<Color> lightColors;
  LightType lightType;
  float pointBeam;
  float beamAmbience;
  std::optional<PeriodicFunction<float>> lightFlickering;

  String soundEffect;
  float soundEffectRangeMultiplier;

  List<PersistentStatusEffect> statusEffects;
  Json touchDamageConfig;

  bool hasObjectItem;
  bool retainObjectParametersInItem;

  bool smashable;
  bool smashOnBreak;
  bool unbreakable;
  String smashDropPool;
  List<List<ItemDescriptor>> smashDropOptions;
  StringList smashSoundOptions;
  JsonArray smashParticles;

  String breakDropPool;
  List<List<ItemDescriptor>> breakDropOptions;

  TileDamageParameters tileDamageParameters;
  float damageShakeMagnitude;
  String damageMaterialKind;

  EntityDamageTeam damageTeam;

  std::optional<float> minimumLiquidLevel;
  std::optional<float> maximumLiquidLevel;
  float liquidCheckInterval;

  float health;

  Json animationConfig;

  List<std::shared_ptr<ObjectOrientation>> orientations;

  // If true, the object will root - it will prevent the blocks it is
  // anchored to from being destroyed directly, and damage from those
  // blocks will be redirected to the object
  bool rooting;

  bool biomePlaced;
};

class ObjectDatabase {
public:
  static List<Vec2I> scanImageSpaces(std::shared_ptr<Image const> const& image, Vec2F const& position, float fillLimit, bool flip = false);
  [[nodiscard]] static std::expected<Json, StarException> parseTouchDamage(Root& root, String const& path, Json const& touchDamage);
  [[nodiscard]] static std::expected<List<std::shared_ptr<ObjectOrientation>>, StarException> parseOrientations(Root& root, String const& path, Json const& configList, Json const& baseConfig);

  [[nodiscard]] static std::expected<std::shared_ptr<ObjectDatabase>, StarException> create(Root& root);

  void cleanup();

  StringList allObjects() const;
  bool isObject(String const& name) const;

  [[nodiscard]] std::expected<std::shared_ptr<ObjectConfig>, StarException> getConfig(String const& objectName) const;
  [[nodiscard]] std::expected<std::shared_ptr<List<std::shared_ptr<ObjectOrientation>> const>, StarException> getOrientations(String const& objectName) const;

  [[nodiscard]] std::expected<std::shared_ptr<Object>, StarException> createObject(String const& objectName, Json const& objectParameters = JsonObject()) const;
  [[nodiscard]] std::expected<std::shared_ptr<Object>, StarException> diskLoadObject(Json const& diskStore) const;
  [[nodiscard]] std::expected<std::shared_ptr<Object>, StarException> net_load_object(ByteArray const& net_store, NetCompatibilityRules rules = {}) const;

  [[nodiscard]] std::expected<bool, StarException> canPlaceObject(World const& world, Vec2I const& position, String const& objectName) const;
  // If the object is placeable in the given position, creates the given object
  // and sets its position and direction and returns it, otherwise returns
  // null.
  [[nodiscard]] std::expected<std::shared_ptr<Object>, StarException> createForPlacement(World const& world, String const& objectName, Vec2I const& position,
      Direction direction, Json const& parameters = JsonObject()) const;

  [[nodiscard]] std::expected<List<Drawable>, StarException> cursorHintDrawables(World const& world, String const& objectName, Vec2I const& position,
      Direction direction, Json parameters = {}) const;

private:
  explicit ObjectDatabase(Root& root);
  [[nodiscard]] std::expected<std::shared_ptr<ObjectConfig>, StarException> readConfig(String const& path) const;

  Root& m_root;

  StringMap<String> m_paths;
  mutable std::mutex m_cacheMutex;
  mutable HashTtlCache<String, std::shared_ptr<ObjectConfig>> m_configCache;

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
  using ::Star::RecipeDatabase;
  using ::Star::Object;
  using ::Star::ObjectOrientation;
  using ::Star::ObjectConfig;
  using ::Star::ObjectDatabase;
  using ::Star::LuaRoot;
  using ::Star::Rebuilder;
}
