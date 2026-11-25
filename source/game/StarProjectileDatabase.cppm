module;
import std;
import star.format;
import star.json;
import star.net_compatibility;
import star.byte_array;
import star.id_map;
import star.set;
import star.thread;
import star.color;
import star.string;
import star.data_stream;
import star.poly;
import star.list;
import star.rect;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.game_types;
import star.casting;
import star.vector;
import star.bi_map;
import star.strong_typedef;

import star.drawable;
import star.entity_rendering_types;
import star.damage_types;
import star.world_geometry;
import star.damage;
import star.light_source;
import star.entity;
import star.status_types;

namespace Star {

class Root;
class Projectile;

struct ProjectileDatabaseExceptionTag {
  static constexpr std::string_view name() { return "ProjectileDatabaseException"; }
};
using ProjectileDatabaseException = StarError<ProjectileDatabaseExceptionTag, StarException>;

struct ProjectileConfig {
  Json config;

  String typeName;
  String directory;

  String description;

  RectF boundBox;

  Json movementSettings;
  float timeToLive = 0.0f;
  float initialSpeed = 0.0f;
  float acceleration = 0.0f;
  float power = 0.0f;
  PolyF damagePoly;
  bool piercing = false;
  bool falldown = false;
  bool rayCheckToSource = false;
  float knockback = 0.0f;
  bool knockbackDirectional = false;

  // Negative value means infinite bounces.
  int bounces = 0;

  // Happens each time the projectile collides with a solid material
  JsonArray actionOnCollide;

  // Happens when projectile dies in any fashion
  JsonArray actionOnReap;

  // Happens when projectile dies after having collided
  JsonArray actionOnHit;

  // Happens when projectile dies without having collided
  JsonArray actionOnTimeout;

  // Time, repeat flag, and action config
  List<std::tuple<float, bool, Json>> periodic_actions;

  String image;
  unsigned frameNumber = 0;
  float animationCycle = 0.0f;
  bool animationLoops = false;
  unsigned windupFrames = 0;
  bool intangibleWindup = false;
  unsigned winddownFrames = 0;
  bool intangibleWinddown = false;
  bool flippable = false;
  bool orientationLocked = false;

  bool fullbright = false;
  EntityRenderLayer renderLayer;

  Color light_color;
  Vec2F light_position;
  LightType light_type = LightType::Spread;

  String persistentAudio;

  String damageKindImage;

  String damageKind;
  String damageType;
  Json damageTeam;
  std::optional<String> damageRepeatGroup;
  std::optional<float> damageRepeatTimeout;

  List<EphemeralStatusEffect> statusEffects;

  StringSet emitters;

  bool hydrophobic = false;
  bool onlyHitTerrain = false;
  ClientEntityMode client_entity_mode = ClientEntityMode::ClientMasterAllowed;
  bool masterOnly = false;

  StringList scripts;

  List<PersistentStatusEffect> persistentStatusEffects;
  PolyF statusEffectArea;

  Json physics_forces;
  Json physicsCollisions;
};

class ProjectileDatabase {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<ProjectileDatabase>, StarException> create(Root& root);

  StringList allProjectileTypes() const;
  bool isProjectile(String const& typeName) const;

  [[nodiscard]] std::expected<Json, StarException> projectileConfig(String const& type) const;

  [[nodiscard]] std::expected<String, StarException> damageKindImage(String const& type) const;
  [[nodiscard]] std::expected<float, StarException> gravityMultiplier(String const& type) const;

  [[nodiscard]] std::expected<std::shared_ptr<Projectile>, StarException> createProjectile(String const& type, Json const& parameters = JsonObject()) const;
  [[nodiscard]] std::expected<std::shared_ptr<Projectile>, StarException> net_load_projectile(ByteArray const& net_store, NetCompatibilityRules rules = {}) const;

private:
  explicit ProjectileDatabase(Root& root) : m_root(root) {}
  [[nodiscard]] std::expected<std::shared_ptr<ProjectileConfig>, StarException> readConfig(String const& path);

  Root& m_root;

  StringMap<std::shared_ptr<ProjectileConfig>> m_configs;
};

}

export module star.projectile_database;

export namespace Star {
  using ::Star::ProjectileDatabaseExceptionTag;
  using ::Star::ProjectileDatabaseException;
  using ::Star::ProjectileConfig;
  using ::Star::Projectile;
  using ::Star::ProjectileDatabase;
}
