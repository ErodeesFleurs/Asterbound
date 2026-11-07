module;
import std;
import star.type_identity;
import star.string;
import star.list;
import star.map;
import star.format;
import star.json;
import star.image;
import star.data_stream;
import star.bi_map;
import star.poly;
import star.vector;
import star.game_types;
import star.liquid_types;
import star.material_types;
import star.random;
import star.set;
import star.thread;
import star.ordered_map;
import star.lru_cache;

import star.item_descriptor;
import star.world_geometry;

import star.rect;
namespace Star {
class Root;

struct DungeonExceptionTag {
  static constexpr std::string_view name() { return "DungeonException"; }
};
using DungeonException = StarError<DungeonExceptionTag, StarException>;

class DungeonDefinition;

class DungeonGeneratorWorldFacade {
public:
  virtual ~DungeonGeneratorWorldFacade() = default;

  // Hint that the given rectangular region is dungeon generated, and thus
  // would not receive the normal entity generation steps.
  [[nodiscard]] virtual std::expected<void, StarException> markRegion(RectI const& region) = 0;
  // Mark the region as needing terrain to properly integrate with the dungeon
  [[nodiscard]] virtual std::expected<void, StarException> markTerrain(PolyF const& region) = 0;
  // Mark the region as needing space to properly integrate with the dungeon
  [[nodiscard]] virtual std::expected<void, StarException> markSpace(PolyF const& region) = 0;

  [[nodiscard]] virtual std::expected<void, StarException> setForegroundMaterial(Vec2I const& position, MaterialId material, MaterialHue hueshift, MaterialColorVariant colorVariant) = 0;
  [[nodiscard]] virtual std::expected<void, StarException> setBackgroundMaterial(Vec2I const& position, MaterialId material, MaterialHue hueshift, MaterialColorVariant colorVariant) = 0;
  [[nodiscard]] virtual std::expected<void, StarException> setForegroundMod(Vec2I const& position, ModId mod, MaterialHue hueshift) = 0;
  [[nodiscard]] virtual std::expected<void, StarException> setBackgroundMod(Vec2I const& position, ModId mod, MaterialHue hueshift) = 0;
  [[nodiscard]] virtual std::expected<void, StarException> placeObject(Vec2I const& pos, String const& objectName, Star::Direction direction, Json const& parameters) = 0;
  [[nodiscard]] virtual std::expected<void, StarException> placeVehicle(Vec2F const& pos, String const& vehicleName, Json const& parameters) = 0;
  [[nodiscard]] virtual std::expected<void, StarException> placeSurfaceBiomeItems(Vec2I const& pos) = 0;
  [[nodiscard]] virtual std::expected<void, StarException> placeBiomeTree(Vec2I const& pos) = 0;
  [[nodiscard]] virtual std::expected<void, StarException> addDrop(Vec2F const& position, ItemDescriptor const& item) = 0;
  [[nodiscard]] virtual std::expected<void, StarException> spawnNpc(Vec2F const& position, Json const& parameters) = 0;
  [[nodiscard]] virtual std::expected<void, StarException> spawnStagehand(Vec2F const& position, Json const& definition) = 0;
  [[nodiscard]] virtual std::expected<void, StarException> setLiquid(Vec2I const& pos, LiquidStore const& liquid) = 0;
  [[nodiscard]] virtual std::expected<void, StarException> connectWireGroup(List<Vec2I> const& wireGroup) = 0;
  [[nodiscard]] virtual std::expected<void, StarException> setTileProtection(DungeonId dungeonId, bool isProtected) = 0;
  [[nodiscard]] virtual std::expected<bool, StarException> checkSolid(Vec2I const& position, TileLayer layer) = 0;
  [[nodiscard]] virtual std::expected<bool, StarException> checkOpen(Vec2I const& position, TileLayer layer) = 0;
  [[nodiscard]] virtual std::expected<bool, StarException> checkOceanLiquid(Vec2I const& position) = 0;
  [[nodiscard]] virtual std::expected<DungeonId, StarException> getDungeonIdAt(Vec2I const& position) = 0;
  [[nodiscard]] virtual std::expected<void, StarException> setDungeonIdAt(Vec2I const& position, DungeonId dungeonId) = 0;
  [[nodiscard]] virtual std::expected<void, StarException> clearTileEntities(RectI const& bounds, Set<Vec2I> const& positions, bool clearAnchoredObjects) = 0;

  virtual WorldGeometry getWorldGeometry() const = 0;

  [[nodiscard]] virtual std::expected<void, StarException> setPlayerStart(Vec2F const& startPosition) = 0;
};

namespace Dungeon {
  class Part;
  struct Tile;

  class DungeonGeneratorWriter {
    Root& m_root;
  public:
    Root& root() const { return m_root; }
    DungeonGeneratorWriter(Root& root, std::shared_ptr<DungeonGeneratorWorldFacade> facade, std::optional<int> terrainMarkingSurfaceLevel, std::optional<int> terrainSurfaceSpaceExtends);

    Vec2I wrapPosition(Vec2I const& pos) const;

    void setMarkDungeonId(std::optional<DungeonId> markDungeonId = {});

    void requestLiquid(Vec2I const& pos, LiquidStore const& liquid);
    void setLiquid(Vec2I const& pos, LiquidStore const& liquid);
    void setForegroundMaterial(Vec2I const& position, MaterialId material, MaterialHue hueshift, MaterialColorVariant colorVariant);
    void setBackgroundMaterial(Vec2I const& position, MaterialId material, MaterialHue hueshift, MaterialColorVariant colorVariant);
    void setForegroundMod(Vec2I const& position, ModId mod, MaterialHue hueshift);
    void setBackgroundMod(Vec2I const& position, ModId mod, MaterialHue hueshift);
    bool needsForegroundBiomeMod(Vec2I const& position);
    bool needsBackgroundBiomeMod(Vec2I const& position);
    void placeObject(Vec2I const& position, String const& objectType, Direction direction, Json const& parameters);
    void placeVehicle(Vec2F const& pos, String const& vehicleName, Json const& parameters);
    void placeSurfaceBiomeItems(Vec2I const& pos);
    void placeBiomeTree(Vec2I const& pos);
    void addDrop(Vec2F const& position, ItemDescriptor const& item);
    void requestWire(Vec2I const& position, String const& wireGroup, bool partLocal);
    void spawnNpc(Vec2F const& pos, Json const& definition);
    void spawnStagehand(Vec2F const& pos, Json const& definition);
    [[nodiscard]] std::expected<void, StarException> setPlayerStart(Vec2F const& startPosition);
    [[nodiscard]] std::expected<bool, StarException> checkSolid(Vec2I position, TileLayer layer);
    [[nodiscard]] std::expected<bool, StarException> checkOpen(Vec2I position, TileLayer layer);
    [[nodiscard]] std::expected<bool, StarException> checkLiquid(Vec2I const& position);
    [[nodiscard]] std::expected<bool, StarException> otherDungeonPresent(Vec2I position);
    void setDungeonId(Vec2I const& pos, DungeonId dungeonId);
    void markPosition(Vec2F const& pos);
    void markPosition(Vec2I const& pos);
    void finishPart();

    [[nodiscard]] std::expected<void, StarException> clearTileEntities(RectI const& bounds, Set<Vec2I> const& positions, bool clearAnchoredObjects);

    [[nodiscard]] std::expected<void, StarException> flushLiquid();
    [[nodiscard]] std::expected<void, StarException> flush();

    List<RectI> boundingBoxes() const;

    void reset();

  private:
    struct Material {
      MaterialId material;
      MaterialHue hueshift;
      MaterialColorVariant colorVariant;
    };

    struct Mod {
      ModId mod;
      MaterialHue hueshift;
    };

    struct ObjectSettings {
      ObjectSettings() : direction() {}
      ObjectSettings(String const& objectName, Direction direction, Json const& parameters)
        : objectName(objectName), direction(direction), parameters(parameters) {}

      String objectName;
      Direction direction;
      Json parameters;
    };

    std::shared_ptr<DungeonGeneratorWorldFacade> m_facade;
    std::optional<int> m_terrainMarkingSurfaceLevel;
    std::optional<int> m_terrainSurfaceSpaceExtends;

    Map<Vec2I, LiquidStore> m_pendingLiquids;

    Map<Vec2I, Material> m_foregroundMaterial;
    Map<Vec2I, Material> m_backgroundMaterial;
    Map<Vec2I, Mod> m_foregroundMod;
    Map<Vec2I, Mod> m_backgroundMod;

    Map<Vec2I, ObjectSettings> m_objects;
    Map<Vec2F, std::pair<String, Json>> m_vehicles;
    Set<Vec2I> m_biomeTrees;
    Set<Vec2I> m_biomeItems;
    Map<Vec2F, ItemDescriptor> m_drops;
    Map<Vec2F, Json> m_npcs;
    Map<Vec2F, Json> m_stagehands;
    Map<Vec2I, DungeonId> m_dungeonIds;

    Map<Vec2I, LiquidStore> m_liquids;

    StringMap<Set<Vec2I>> m_globalWires;
    List<Set<Vec2I>> m_localWires;
    StringMap<Set<Vec2I>> m_openLocalWires;

    std::optional<DungeonId> m_markDungeonId;
    RectI m_currentBounds;
    List<RectI> m_boundingBoxes;
  };

  class Rule {
  public:
    virtual void const* queryType(TypeKey key) const noexcept { return queryTypeOf<Rule>(this, key); }
    virtual std::string_view dynamicTypeName() const noexcept { return Star::typeName<Rule>(); }

    [[nodiscard]] static std::expected<std::optional<std::shared_ptr<Rule const>>, StarException> parse(Root& root, Json const& rule);
    [[nodiscard]] static std::expected<List<std::shared_ptr<Rule const>>, StarException> readRules(Root& root, Json const& rules);

    virtual ~Rule() = default;

    [[nodiscard]] virtual std::expected<bool, StarException> checkTileCanPlace(Vec2I position, DungeonGeneratorWriter& writer) const;

    virtual bool overdrawable() const;
    virtual bool ignorePartMaximum() const;
    virtual bool allowSpawnCount(int currentCount) const;

    virtual bool doesNotConnectToPart(String const& name) const;
    virtual bool checkPartCombinationsAllowed(StringMap<int> const& placementCounter) const;

    virtual bool requiresOpen() const;
    virtual bool requiresSolid() const;
    virtual bool requiresLiquid() const;

  protected:
    Rule() = default;
  };

  class WorldGenMustContainAirRule : public Rule {
  public:
    void const* queryType(TypeKey key) const noexcept override { return queryTypeOf<WorldGenMustContainAirRule, Rule>(this, key); }
    std::string_view dynamicTypeName() const noexcept override { return Star::typeName<WorldGenMustContainAirRule>(); }

    WorldGenMustContainAirRule(TileLayer layer) : layer(layer) {}

    [[nodiscard]] std::expected<bool, StarException> checkTileCanPlace(Vec2I position,
                                                                       DungeonGeneratorWriter& writer) const override;

    bool requiresOpen() const override { return true; }

    TileLayer layer;
  };

  class WorldGenMustContainSolidRule : public Rule {
  public:
    void const* queryType(TypeKey key) const noexcept override { return queryTypeOf<WorldGenMustContainSolidRule, Rule>(this, key); }
    std::string_view dynamicTypeName() const noexcept override { return Star::typeName<WorldGenMustContainSolidRule>(); }

    WorldGenMustContainSolidRule(TileLayer layer) : layer(layer) {}

    [[nodiscard]] std::expected<bool, StarException> checkTileCanPlace(Vec2I position,
                                                                       DungeonGeneratorWriter& writer) const override;

    bool requiresSolid() const override { return true; }

    TileLayer layer;
  };

  class WorldGenMustContainLiquidRule : public Rule {
  public:
    void const* queryType(TypeKey key) const noexcept override { return queryTypeOf<WorldGenMustContainLiquidRule, Rule>(this, key); }
    std::string_view dynamicTypeName() const noexcept override { return Star::typeName<WorldGenMustContainLiquidRule>(); }

    WorldGenMustContainLiquidRule() = default;

    [[nodiscard]] std::expected<bool, StarException> checkTileCanPlace(Vec2I position,
                                                                       DungeonGeneratorWriter& writer) const override;

    bool requiresLiquid() const override { return true; }
  };

  class WorldGenMustNotContainLiquidRule : public Rule {
  public:
    void const* queryType(TypeKey key) const noexcept override { return queryTypeOf<WorldGenMustNotContainLiquidRule, Rule>(this, key); }
    std::string_view dynamicTypeName() const noexcept override { return Star::typeName<WorldGenMustNotContainLiquidRule>(); }

    WorldGenMustNotContainLiquidRule() = default;

    [[nodiscard]] std::expected<bool, StarException> checkTileCanPlace(Vec2I position,
                                                                       DungeonGeneratorWriter& writer) const override;
  };

  class AllowOverdrawingRule : public Rule {
  public:
    void const* queryType(TypeKey key) const noexcept override { return queryTypeOf<AllowOverdrawingRule, Rule>(this, key); }
    std::string_view dynamicTypeName() const noexcept override { return Star::typeName<AllowOverdrawingRule>(); }

    AllowOverdrawingRule() = default;

    bool overdrawable() const override { return true; }
  };

  class IgnorePartMaximumRule : public Rule {
  public:
    void const* queryType(TypeKey key) const noexcept override { return queryTypeOf<IgnorePartMaximumRule, Rule>(this, key); }
    std::string_view dynamicTypeName() const noexcept override { return Star::typeName<IgnorePartMaximumRule>(); }

    IgnorePartMaximumRule() = default;

    bool ignorePartMaximum() const override { return true; }
  };

  class MaxSpawnCountRule : public Rule {
  public:
    void const* queryType(TypeKey key) const noexcept override { return queryTypeOf<MaxSpawnCountRule, Rule>(this, key); }
    std::string_view dynamicTypeName() const noexcept override { return Star::typeName<MaxSpawnCountRule>(); }

    [[nodiscard]] static std::expected<std::shared_ptr<MaxSpawnCountRule>, StarException> create(Json const& rule);

    bool allowSpawnCount(int currentCount) const override { return currentCount < m_maxCount; }

  private:
    MaxSpawnCountRule() = default;
    int m_maxCount;
  };

  class DoNotConnectToPartRule : public Rule {
  public:
    void const* queryType(TypeKey key) const noexcept override { return queryTypeOf<DoNotConnectToPartRule, Rule>(this, key); }
    std::string_view dynamicTypeName() const noexcept override { return Star::typeName<DoNotConnectToPartRule>(); }

    [[nodiscard]] static std::expected<std::shared_ptr<DoNotConnectToPartRule>, StarException> create(Json const& rule);

    bool doesNotConnectToPart(String const& name) const override { return m_partNames.contains(name); }

  private:
    DoNotConnectToPartRule() = default;
    StringSet m_partNames;
  };

  class DoNotCombineWithRule : public Rule {
  public:
    void const* queryType(TypeKey key) const noexcept override { return queryTypeOf<DoNotCombineWithRule, Rule>(this, key); }
    std::string_view dynamicTypeName() const noexcept override { return Star::typeName<DoNotCombineWithRule>(); }

    [[nodiscard]] static std::expected<std::shared_ptr<DoNotCombineWithRule>, StarException> create(Json const& rule);

    bool checkPartCombinationsAllowed(StringMap<int> const& placementCounter) const override {
      for (auto const& part : m_parts) {
        if (auto entry = placementCounter.find(part); entry != placementCounter.end() && entry->second > 0)
          return false;
      }
      return true;
    }

  private:
    DoNotCombineWithRule() = default;
    StringSet m_parts;
  };

  enum class Phase {
    ClearPhase,
    WallPhase,
    ModsPhase,
    ObjectPhase,
    BiomeTreesPhase,
    BiomeItemsPhase,
    WirePhase,
    ItemPhase,
    NpcPhase,
    DungeonIdPhase
  };

  class Brush {
  public:
    [[nodiscard]] static std::expected<std::shared_ptr<Brush const>, StarException> parse(Root& root, Json const& brush);
    [[nodiscard]] static std::expected<List<std::shared_ptr<Brush const>>, StarException> readBrushes(Root& root, Json const& brushes);

    virtual ~Brush() = default;

    [[nodiscard]] virtual std::expected<void, StarException> paint(Vec2I position, Phase phase, DungeonGeneratorWriter& writer) const = 0;

  protected:
    Brush() = default;
  };

  class RandomBrush : public Brush {
  public:
    [[nodiscard]] static std::expected<std::shared_ptr<RandomBrush>, StarException> create(Root& root, Json const& brush);

    [[nodiscard]] std::expected<void, StarException> paint(Vec2I position, Phase phase,
                                                           DungeonGeneratorWriter& writer) const override;

  private:
    RandomBrush() = default;
    List<std::shared_ptr<Brush const>> m_brushes;
    std::int64_t m_seed;
  };

  class ClearBrush : public Brush {
  public:
    ClearBrush() = default;

    [[nodiscard]] std::expected<void, StarException> paint(Vec2I position, Phase phase,
                                                           DungeonGeneratorWriter& writer) const override;
  };

  class FrontBrush : public Brush {
  public:
    FrontBrush(String const& material, std::optional<String> mod, std::optional<float> hueshift, std::optional<float> modhueshift, std::optional<MaterialColorVariant> colorVariant);

    [[nodiscard]] std::expected<void, StarException> paint(Vec2I position, Phase phase,
                                                           DungeonGeneratorWriter& writer) const override;

  private:
    String m_material;
    MaterialHue m_materialHue;
    MaterialColorVariant m_materialColorVariant;
    std::optional<String> m_mod;
    MaterialHue m_modHue;
  };

  class BackBrush : public Brush {
  public:
    BackBrush(String const& material, std::optional<String> mod, std::optional<float> hueshift, std::optional<float> modhueshift, std::optional<MaterialColorVariant> colorVariant);

    [[nodiscard]] std::expected<void, StarException> paint(Vec2I position, Phase phase,
                                                           DungeonGeneratorWriter& writer) const override;

  private:
    String m_material;
    MaterialHue m_materialHue;
    MaterialColorVariant m_materialColorVariant;
    std::optional<String> m_mod;
    MaterialHue m_modHue;
  };

  class ObjectBrush : public Brush {
  public:
    ObjectBrush(String const& object, Star::Direction direction, Json const& parameters);

    [[nodiscard]] std::expected<void, StarException> paint(Vec2I position, Phase phase,
                                                           DungeonGeneratorWriter& writer) const override;

  private:
    String m_object;
    Star::Direction m_direction;
    Json m_parameters;
  };

  class VehicleBrush : public Brush {
  public:
    VehicleBrush(String const& vehicle, Json const& parameters);

    [[nodiscard]] std::expected<void, StarException> paint(Vec2I position, Phase phase,
                                                           DungeonGeneratorWriter& writer) const override;

  private:
    String m_vehicle;
    Json m_parameters;
  };

  class BiomeItemsBrush : public Brush {
  public:
    BiomeItemsBrush() = default;

    [[nodiscard]] std::expected<void, StarException> paint(Vec2I position, Phase phase,
                                                           DungeonGeneratorWriter& writer) const override;
  };

  class BiomeTreeBrush : public Brush {
  public:
    BiomeTreeBrush() = default;

    [[nodiscard]] std::expected<void, StarException> paint(Vec2I position, Phase phase,
                                                           DungeonGeneratorWriter& writer) const override;
  };

  class ItemBrush : public Brush {
  public:
    ItemBrush(ItemDescriptor const& item);

    [[nodiscard]] std::expected<void, StarException> paint(Vec2I position, Phase phase,
                                                           DungeonGeneratorWriter& writer) const override;

  private:
    ItemDescriptor m_item;
  };

  class NpcBrush : public Brush {
  public:
    [[nodiscard]] static std::expected<std::shared_ptr<NpcBrush>, StarException> create(Root& root, Json const& brush);

    [[nodiscard]] std::expected<void, StarException> paint(Vec2I position, Phase phase,
                                                           DungeonGeneratorWriter& writer) const override;

  private:
    NpcBrush() = default;
    Json m_npc;
  };

  class StagehandBrush : public Brush {
  public:
    StagehandBrush(Json const& definition);

    [[nodiscard]] std::expected<void, StarException> paint(Vec2I position, Phase phase,
                                                           DungeonGeneratorWriter& writer) const override;

  private:
    Json m_definition;
  };

  class DungeonIdBrush : public Brush {
  public:
    DungeonIdBrush(DungeonId dungeonId);

    [[nodiscard]] std::expected<void, StarException> paint(Vec2I position, Phase phase,
                                                           DungeonGeneratorWriter& writer) const override;

  private:
    DungeonId m_dungeonId;
  };

  class SurfaceBrush : public Brush {
  public:
    SurfaceBrush(std::optional<int> variant, std::optional<String> mod);

    [[nodiscard]] std::expected<void, StarException> paint(Vec2I position, Phase phase,
                                                           DungeonGeneratorWriter& writer) const override;

  private:
    int m_variant;
    std::optional<String> m_mod;
  };

  class SurfaceBackgroundBrush : public Brush {
  public:
    SurfaceBackgroundBrush(std::optional<int> variant, std::optional<String> mod);

    [[nodiscard]] std::expected<void, StarException> paint(Vec2I position, Phase phase,
                                                           DungeonGeneratorWriter& writer) const override;

  private:
    int m_variant;
    std::optional<String> m_mod;
  };

  class LiquidBrush : public Brush {
  public:
    LiquidBrush(String const& liquidName, float quantity, bool source);

    [[nodiscard]] std::expected<void, StarException> paint(Vec2I position, Phase phase,
                                                           DungeonGeneratorWriter& writer) const override;

  private:
    String m_liquid;
    float m_quantity;
    bool m_source;
  };

  class WireBrush : public Brush {
  public:
    WireBrush(String wireGroup, bool partLocal) : m_wireGroup(std::move(wireGroup)), m_partLocal(partLocal) {}

    [[nodiscard]] std::expected<void, StarException> paint(Vec2I position, Phase phase,
                                                           DungeonGeneratorWriter& writer) const override;

  private:
    String m_wireGroup;
    bool m_partLocal;
  };

  class PlayerStartBrush : public Brush {
  public:
    [[nodiscard]] std::expected<void, StarException> paint(Vec2I position, Phase phase,
                                                           DungeonGeneratorWriter& writer) const override;
  };

  // InvalidBrush reports an error when it is painted. This brush is used on
  // tiles
  // that represent objects that have been removed from the game.
  class InvalidBrush : public Brush {
  public:
    InvalidBrush(std::optional<String> nameHint);

    [[nodiscard]] std::expected<void, StarException> paint(Vec2I position, Phase phase,
                                                           DungeonGeneratorWriter& writer) const override;

  private:
    std::optional<String> m_nameHint;
  };

  enum class Direction : std::uint8_t {
    Left = 0,
    Right = 1,
    Up = 2,
    Down = 3,
    Unknown = 4,
    Any = 5
  };
  inline constexpr auto DungeonDirectionNames =
    sortedEnumNames(std::to_array<std::pair<Direction, std::string_view>>({{Direction::Left, "left"},
                                                                           {Direction::Right, "right"},
                                                                           {Direction::Up, "up"},
                                                                           {Direction::Down, "down"},
                                                                           {Direction::Unknown, "unknown"},
                                                                           {Direction::Any, "any"}}));

  class Connector {
  public:
    Connector(Part const& part, String value, bool forwardOnly, Direction direction, Vec2I offset);

    [[nodiscard]] std::expected<bool, StarException> connectsTo(std::shared_ptr<Connector const> connector) const;

    String value() const;
    Vec2I positionAdjustment() const;
    Part const& part() const;
    Vec2I offset() const;

  private:
    String m_value;
    bool m_forwardOnly;
    Direction m_direction;
    Vec2I m_offset;
    std::reference_wrapper<Part const> m_part;
  };

  // Callback errors propagate through readers, scans and placement. A successful
  // true still means the original early exit, not a generation failure.
  using TileCallback = std::function<std::expected<bool, StarException>(Vec2I, Tile const&)>;

  class PartReader {
  public:
    virtual ~PartReader() = default;

    [[nodiscard]] virtual std::expected<void, StarException> readAsset(String const& asset) = 0;

    // Returns the dimensions of the part
    virtual Vec2U size() const = 0;

    // Iterate over every tile in every layer of the part.
    // The callback receives the position of the tile (within the part), and
    // the tile at that position.
    // The callback can return true to exit from the loop early.
    [[nodiscard]] virtual std::expected<void, StarException> forEachTile(TileCallback const& callback) const = 0;

    // Calls the callback with only the tiles at the given position.
    [[nodiscard]] virtual std::expected<void, StarException> forEachTileAt(Vec2I pos, TileCallback const& callback) const = 0;

  protected:
    PartReader() = default;
  };

  class Part {
    Root& m_root;
  public:
    Root& root() const { return m_root; }
    [[nodiscard]] static std::expected<std::shared_ptr<Part>, StarException> create(Root& root, DungeonDefinition const& dungeon, Json const& part, std::shared_ptr<PartReader> reader);

    String const& name() const;
    Vec2U size() const;
    Vec2I anchorPoint() const;
    float chance() const;
    bool markDungeonId() const;
    std::optional<float> minimumThreatLevel() const;
    std::optional<float> maximumThreatLevel() const;
    bool clearAnchoredObjects() const;
    [[nodiscard]] std::expected<int, StarException> placementLevelConstraint() const;
    bool ignoresPartMaximum() const;
    bool allowsPlacement(int currentPlacementCount) const;
    List<std::shared_ptr<Connector const>> const& connections() const;
    bool doesNotConnectTo(Part const& part) const;
    bool checkPartCombinationsAllowed(StringMap<int> const& placementCounts) const;
    [[nodiscard]] std::expected<bool, StarException> collidesWithPlaces(Vec2I pos, Set<Vec2I>& places) const;

    [[nodiscard]] std::expected<bool, StarException> canPlace(Vec2I pos, DungeonGeneratorWriter& writer) const;

    [[nodiscard]] std::expected<void, StarException> place(Vec2I pos, Set<Vec2I> const& places, DungeonGeneratorWriter& writer) const;

    [[nodiscard]] std::expected<void, StarException> forEachTile(TileCallback const& callback) const;

  private:
    explicit Part(Root& root) : m_root(root) {}
    [[nodiscard]] std::expected<void, StarException> placePhase(Vec2I pos, Phase phase, Set<Vec2I> const& places, DungeonGeneratorWriter& writer) const;

    [[nodiscard]] std::expected<bool, StarException> tileUsesPlaces(Vec2I pos) const;
    [[nodiscard]] std::expected<Direction, StarException> pickByEdge(Vec2I position, Vec2U size) const;
    [[nodiscard]] std::expected<Direction, StarException> pickByNeighbours(Vec2I pos) const;
    [[nodiscard]] std::expected<void, StarException> scanConnectors();
    [[nodiscard]] std::expected<void, StarException> scanAnchor();

    std::shared_ptr<PartReader const> m_reader;

    String m_name;
    List<std::shared_ptr<Rule const>> m_rules;

    List<std::shared_ptr<Connector const>> m_connections;
    Vec2I m_anchorPoint;
    bool m_overrideAllowAlways;
    std::optional<float> m_minimumThreatLevel;
    std::optional<float> m_maximumThreatLevel;
    bool m_clearAnchoredObjects;
    Vec2U m_size;
    float m_chance;
    bool m_markDungeonId;
  };

  struct TileConnector {
    String value;
    bool forwardOnly;
    Direction direction = Direction::Unknown;
  };

  struct Tile {
    [[nodiscard]] std::expected<bool, StarException> canPlace(Vec2I position, DungeonGeneratorWriter& writer) const;
    [[nodiscard]] std::expected<void, StarException> place(Vec2I position, Phase phase, DungeonGeneratorWriter& writer) const;
    bool usesPlaces() const;
    bool modifiesPlaces() const;
    bool collidesWithPlaces() const;
    bool requiresOpen() const;
    bool requiresSolid() const;
    bool requiresLiquid() const;

    List<std::shared_ptr<Brush const>> brushes;
    List<std::shared_ptr<Rule const>> rules;
    std::optional<TileConnector> connector;
  };
}

class DungeonDefinition {
  Root& m_root;
public:
  // Resource parsing completes before publishing the owned definition or parts.
  Root& root() const { return m_root; }
    [[nodiscard]] static std::expected<std::shared_ptr<DungeonDefinition>, StarException> create(Root& root, JsonObject const& definition, String const& directory);

  JsonObject metadata() const;
  String directory() const;
  String name() const;
  String displayName() const;
  bool isProtected() const;
  std::optional<float> gravity() const;
  std::optional<bool> breathable() const;
  StringMap<std::shared_ptr<Dungeon::Part const>> const& parts() const;

  List<String> const& anchors() const;
  std::optional<Json> const& optTileset() const;
  int maxParts() const;
  int maxRadius() const;
  int extendSurfaceFreeSpace() const;

  JsonObject metaData() const;

private:
  explicit DungeonDefinition(Root& root) : m_root(root) {}
  JsonObject m_metadata;
  String m_directory;
  String m_name;
  String m_displayName;
  String m_species;
  bool m_isProtected;
  List<std::shared_ptr<Dungeon::Rule const>> m_rules;
  StringMap<std::shared_ptr<Dungeon::Part const>> m_parts;
  List<String> m_anchors;
  std::optional<Json> m_tileset;

  int m_maxRadius;
  int m_maxParts;
  int m_extendSurfaceFreeSpace;

  std::optional<float> m_gravity;
  std::optional<bool> m_breathable;
};

class DungeonDefinitions {
  Root& m_root;
public:
  Root& root() const { return m_root; }
    [[nodiscard]] static std::expected<std::shared_ptr<DungeonDefinitions>, StarException> create(Root& root);

  [[nodiscard]] std::expected<std::shared_ptr<DungeonDefinition const>, StarException> get(String const& name) const;
  [[nodiscard]] std::expected<JsonObject, StarException> getMetadata(String const& name) const;

private:
  explicit DungeonDefinitions(Root& root);
  [[nodiscard]] std::expected<std::shared_ptr<DungeonDefinition>, StarException> readDefinition(String const& path) const;

  StringMap<String> m_paths;
  mutable std::mutex m_cacheMutex;
  mutable HashLruCache<String, std::shared_ptr<DungeonDefinition>> m_definitionCache;
};

class DungeonGenerator {
  Root& m_root;
public:
  Root& root() const { return m_root; }
    [[nodiscard]] static std::expected<std::shared_ptr<DungeonGenerator>, StarException> create(Root& root, String const& dungeonName, std::uint64_t seed, float threatLevel, std::optional<DungeonId> dungeonId);

  [[nodiscard]] std::expected<std::optional<std::pair<List<RectI>, Set<Vec2I>>>, StarException> generate(std::shared_ptr<DungeonGeneratorWorldFacade> facade, Vec2I position, bool markSurfaceAndTerrain, bool forcePlacement);

  [[nodiscard]] std::expected<std::pair<List<RectI>, Set<Vec2I>>, StarException> buildDungeon(std::shared_ptr<Dungeon::Part const> anchor, Vec2I pos, Dungeon::DungeonGeneratorWriter& writer, bool forcePlacement);
  [[nodiscard]] std::expected<std::shared_ptr<Dungeon::Part const>, StarException> pickAnchor();
  [[nodiscard]] std::expected<List<std::shared_ptr<Dungeon::Connector const>>, StarException> findConnectablePart(std::shared_ptr<Dungeon::Connector const> connector) const;

  std::shared_ptr<DungeonDefinition const> definition() const;

private:
  DungeonGenerator(Root& root, std::uint64_t seed, float threatLevel, std::optional<DungeonId> dungeonId);
  std::shared_ptr<DungeonDefinition const> m_def;

  RandomSource m_rand;
  float m_threatLevel;
  std::optional<DungeonId> m_dungeonId;
};

}

export module star.dungeon_generator;

export namespace Star {
  using ::Star::DungeonExceptionTag;
  using ::Star::DungeonException;
  using ::Star::DungeonGeneratorWorldFacade;
  using ::Star::DungeonDefinition;
  using ::Star::DungeonDefinitions;
  using ::Star::DungeonGenerator;
}

export namespace Star::Dungeon {
  using ::Star::Dungeon::DungeonGeneratorWriter;
  using ::Star::Dungeon::PartReader;
  using ::Star::Dungeon::Part;
  using ::Star::Dungeon::Rule;
  using ::Star::Dungeon::Brush;
  using ::Star::Dungeon::Tile;
  using ::Star::Dungeon::Connector;
  using ::Star::Dungeon::WorldGenMustContainAirRule;
  using ::Star::Dungeon::WorldGenMustContainSolidRule;
  using ::Star::Dungeon::WorldGenMustContainLiquidRule;
  using ::Star::Dungeon::WorldGenMustNotContainLiquidRule;
  using ::Star::Dungeon::AllowOverdrawingRule;
  using ::Star::Dungeon::IgnorePartMaximumRule;
  using ::Star::Dungeon::MaxSpawnCountRule;
  using ::Star::Dungeon::DoNotConnectToPartRule;
  using ::Star::Dungeon::DoNotCombineWithRule;
  using ::Star::Dungeon::Phase;
  using ::Star::Dungeon::RandomBrush;
  using ::Star::Dungeon::ClearBrush;
  using ::Star::Dungeon::FrontBrush;
  using ::Star::Dungeon::BackBrush;
  using ::Star::Dungeon::ObjectBrush;
  using ::Star::Dungeon::VehicleBrush;
  using ::Star::Dungeon::BiomeItemsBrush;
  using ::Star::Dungeon::BiomeTreeBrush;
  using ::Star::Dungeon::ItemBrush;
  using ::Star::Dungeon::NpcBrush;
  using ::Star::Dungeon::StagehandBrush;
  using ::Star::Dungeon::DungeonIdBrush;
  using ::Star::Dungeon::SurfaceBrush;
  using ::Star::Dungeon::SurfaceBackgroundBrush;
  using ::Star::Dungeon::LiquidBrush;
  using ::Star::Dungeon::WireBrush;
  using ::Star::Dungeon::PlayerStartBrush;
  using ::Star::Dungeon::InvalidBrush;
  using ::Star::Dungeon::Direction;
  using ::Star::Dungeon::DungeonDirectionNames;
  using ::Star::Dungeon::TileCallback;
  using ::Star::Dungeon::TileConnector;
}
