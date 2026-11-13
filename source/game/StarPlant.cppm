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
namespace Star {
class Root;

class RenderCallback;
struct TreeVariant;
struct GrassVariant;
struct BushVariant;

struct PlantExceptionTag {
  static constexpr std::string_view name() { return "PlantException"; }
};
using PlantException = StarError<PlantExceptionTag, StarException>;

class Plant : public virtual TileEntity {
public:
  void const* query_type(TypeKey key) const noexcept override { return query_type_of<Plant, TileEntity>(this, key); }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<Plant>(); }

  // TODO: For right now the space scan threshold is hard-coded, but should be
  // configurable in the future
  static float const PlantScanThreshold;

  enum RotationType {
    DontRotate,
    RotateBranch,
    RotateLeaves,
    RotateCrownBranch,
    RotateCrownLeaves
  };

  static std::expected<EnumMap<RotationType>, StarException> const RotationTypeNames;

  enum PlantPieceKind {
    None,
    Stem,
    Foliage
  };

  struct PlantPiece {
    PlantPiece();
    AssetPath imagePath;
    String image;
    Vec2U imageSize;
    Vec2F offset;
    int segmentIdx;
    bool structuralSegment;
    PlantPieceKind kind;
    RotationType rotationType;
    float rotationOffset;
    Set<Vec2I> spaces;
    bool flip;
    // no need to serialize
    float zLevel;
  };

  [[nodiscard]] static std::expected<std::shared_ptr<Plant>, StarException> create(Root& rootContext, TreeVariant const& config, std::uint64_t seed);
  [[nodiscard]] static std::expected<std::shared_ptr<Plant>, StarException> create(Root& rootContext, GrassVariant const& config, std::uint64_t seed);
  [[nodiscard]] static std::expected<std::shared_ptr<Plant>, StarException> create(Root& rootContext, BushVariant const& config, std::uint64_t seed);
  [[nodiscard]] static std::expected<std::shared_ptr<Plant>, StarException> create(Root& rootContext, Json const& diskStore);
  [[nodiscard]] static std::expected<std::shared_ptr<Plant>, StarException> create(Root& rootContext, ByteArray const& netStore, NetCompatibilityRules rules = {});

  [[nodiscard]] std::expected<Json, StarException> diskStore() const;
  [[nodiscard]] std::expected<ByteArray, StarException> netStore(NetCompatibilityRules rules = {}) const;

  EntityType entityType() const override;

  [[nodiscard]] std::expected<void, StarException> init(World& world, EntityId entityId, EntityMode mode) override;

  [[nodiscard]] std::expected<String, StarException> description() const override;

  [[nodiscard]] std::expected<std::pair<ByteArray, std::uint64_t>, StarException> writeNetState(std::uint64_t fromVersion = 0, NetCompatibilityRules rules = {}) override;
  [[nodiscard]] std::expected<void, StarException> readNetState(ByteArray data, float interpolationTime = 0.0f, NetCompatibilityRules rules = {}) override;

  [[nodiscard]] std::expected<void, StarException> enableInterpolation(float extrapolationHint) override;
  [[nodiscard]] std::expected<void, StarException> disableInterpolation() override;

  Vec2F position() const override;
  RectF metaBoundBox() const override;

  bool ephemeral() const override;

  [[nodiscard]] std::expected<bool, StarException> shouldDestroy() const override;

  // Forces the plant to check if it has been invalidly placed in some way, and
  // should die.  shouldDie does not, by default, do this expensive calculation
  [[nodiscard]] std::expected<bool, StarException> checkBroken() override;

  // Base tile grid position
  Vec2I tilePosition() const override;
  [[nodiscard]] std::expected<void, StarException> setTilePosition(Vec2I const& tilePosition) override;

  // Spaces this plant currently occupies
  List<Vec2I> spaces() const override;

  // Root blocks for this plant.
  List<Vec2I> roots() const override;

  [[nodiscard]] std::expected<void, StarException> update(float dt, std::uint64_t currentStep) override;

  [[nodiscard]] std::expected<void, StarException> collectRenderEvents(RenderCallback& renderCallback) override;

  [[nodiscard]] std::expected<void, StarException> render(RenderCallback& renderCallback) override;

  [[nodiscard]] std::expected<bool, StarException> damageTiles(List<Vec2I> const& position, Vec2F const& sourcePosition, TileDamage const& tileDamage) override;

  // Central root position
  Vec2I primaryRoot() const;
  // Plant hangs from the ceiling
  bool ceiling() const;

  List<PlantPiece> pieces() const;
  RectF interactiveBoundBox() const override;

private:
  [[nodiscard]] std::expected<void, StarException> initialize(ByteArray const& netStore, NetCompatibilityRules rules);
  [[nodiscard]] std::expected<void, StarException> initialize(Json const& diskStore);
  [[nodiscard]] std::expected<void, StarException> initialize(BushVariant const& config, std::uint64_t seed);
  [[nodiscard]] std::expected<void, StarException> initialize(GrassVariant const& config, std::uint64_t seed);
  [[nodiscard]] std::expected<void, StarException> initialize(TreeVariant const& config, std::uint64_t seed);

  Plant(Root& rootContext);

  [[nodiscard]] std::expected<void, StarException> breakAtPosition(Vec2I const& position, Vec2F const& sourcePosition);
  [[nodiscard]] std::expected<Vec2I, StarException> baseDamagePosition(List<Vec2I> const& positions) const;
  [[nodiscard]] std::expected<bool, StarException> damagable() const;

  [[nodiscard]] std::expected<void, StarException> scanSpacesAndRoots();
  List<PlantPiece> spawnFolliage(String const& key, String const& type);
  float branchRotation(float xPos, float rotoffset, float interpolation) const;
  float m_previousWindTime = 0.0f;
  float m_previousWindLevel = 0.0f;
  float m_presentationDamageXOffset = 0.0f;
  void calcBoundBox();

  [[nodiscard]] std::expected<void, StarException> readPieces(ByteArray pieces);
  [[nodiscard]] std::expected<ByteArray, StarException> writePieces() const;

  [[nodiscard]] std::expected<void, StarException> readPiecesFromJson(Json const& pieces);
  [[nodiscard]] std::expected<Json, StarException> writePiecesToJson() const;

  [[nodiscard]] std::expected<void, StarException> validatePieces();

  [[nodiscard]] std::expected<void, StarException> setupNetStates();
  [[nodiscard]] std::expected<void, StarException> getNetStates();
  [[nodiscard]] std::expected<void, StarException> setNetStates();

  Vec2I m_tilePosition;
  List<Vec2I> m_spaces;
  List<Vec2I> m_roots;
  RectI m_boundBox;

  Json m_descriptions;

  bool m_ephemeral;

  Json m_stemDropConfig;
  Json m_foliageDropConfig;
  Json m_saplingDropConfig;

  List<PlantPiece> m_pieces;
  bool m_piecesUpdated;

  bool m_ceiling;
  bool m_broken;
  bool m_fallsWhenDead;

  float m_windTime;
  float m_windLevel;

  RectF m_metaBoundBox;

  bool m_piecesScanned;

  TileDamageParameters m_tileDamageParameters;
  std::shared_ptr<EntityTileDamageStatus> m_tileDamageStatus;
  float m_tileDamageX;
  float m_tileDamageY;
  bool m_tileDamageEventTrigger;
  bool m_tileDamageEvent;

  NetElementTopGroup m_netGroup;
  NetElementBytes m_piecesNetState;
  NetElementFloat m_tileDamageXNetState;
  NetElementFloat m_tileDamageYNetState;
  NetElementEvent m_tileDamageEventNetState;
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
