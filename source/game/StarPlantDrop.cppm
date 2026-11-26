module;
import std;
import star.format;
import star.type_identity;
import star.string;
import star.json;
import star.id_map;
import star.casting;
import star.vector;
import star.data_stream;
import star.game_types;
import star.strong_typedef;
import star.bi_map;
import star.list;
import star.rpc_promise;
import star.rect;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.set;


import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.tile_damage;
import star.physics_entity;
import star.movement_controller;
import star.mobile_entity;
import star.interaction_types;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.inspectable_entity;
import star.plant;

import star.net_element_basic_fields;
import star.net_element_system;

import star.net_compatibility;
import star.byte_array;
namespace Star {
class Root;

class RenderCallback;

class PlantDrop : public virtual MobileEntity {
public:
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<PlantDrop, MobileEntity>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<PlantDrop>(); }

  [[nodiscard]] static std::expected<std::shared_ptr<PlantDrop>, StarException> create(Root& rootContext, List<Plant::PlantPiece> pieces, Vec2F const& position, Vec2F const& strikeVector, String const& description,
      bool upsideDown, Json stemConfig, Json foliageConfig, Json saplingConfig,
      bool master, float random);
  [[nodiscard]] static std::expected<std::shared_ptr<PlantDrop>, StarException> create(Root& rootContext, ByteArray const& net_store, NetCompatibilityRules rules = {});

  [[nodiscard]] std::expected<ByteArray, StarException> net_store(NetCompatibilityRules rules = {});

  EntityType entity_type() const override;

  [[nodiscard]] std::expected<void, StarException> init(World& world, EntityId entityId, EntityMode mode) override;
  [[nodiscard]] std::expected<void, StarException> uninit() override;

  [[nodiscard]] std::expected<String, StarException> description() const override;

  [[nodiscard]] std::expected<std::pair<ByteArray, std::uint64_t>, StarException> write_net_state(std::uint64_t fromVersion = 0, NetCompatibilityRules rules = {}) override;
  [[nodiscard]] std::expected<std::pair<ByteArray, std::uint64_t>, StarException> snapshot_net_state(NetCompatibilityRules rules = {}) const;
  [[nodiscard]] std::expected<void, StarException> read_net_state(ByteArray const& data, float interpolation_time = 0.0f, NetCompatibilityRules rules = {}) override;

  [[nodiscard]] std::expected<void, StarException> enable_interpolation(float extrapolation_hint = 0.0f) override;
  [[nodiscard]] std::expected<void, StarException> disable_interpolation() override;

  Vec2F position() const override;
  RectF meta_bound_box() const override;

  [[nodiscard]] std::expected<bool, StarException> should_destroy() const override;

  void setPosition();
  void setVelocity(Vec2F const& position);

  RectF collisionRect() const;

  [[nodiscard]] std::expected<void, StarException> update(float dt, std::uint64_t currentStep) override;

  std::uint64_t presentation_epoch() const override;

  [[nodiscard]] std::expected<void, StarException> collectRenderEvents(RenderCallback& render_callback) override;

  [[nodiscard]] std::expected<void, StarException> render(RenderCallback& render_callback) override;
  
  MovementController& movement_controller() override;

private:
  PlantDrop(Root& rootContext);
  [[nodiscard]] std::expected<void, StarException> initialize(ByteArray const& net_store, NetCompatibilityRules rules);
  [[nodiscard]] std::expected<void, StarException> initialize(List<Plant::PlantPiece> pieces, Vec2F const& position, Vec2F const& strikeVector, String const& description,
    bool upsideDown, Json stemConfig, Json foliageConfig, Json saplingConfig, bool master, float random);

  struct PlantDropPiece {
    PlantDropPiece();
    AssetPath image;
    Vec2F offset;
    int segmentIdx;
    Plant::PlantPieceKind kind;
    bool flip;
  };

  [[nodiscard]] std::expected<void, StarException> particleForPlantPart(PlantDropPiece const& piece, String const& mode, Json const& mainConfig, RenderCallback& render_callback);

  NetElementTopGroup m_netGroup;
  String m_description;
  float m_time;
  std::shared_ptr<MovementController> m_movement_controller;
  RectF m_collisionRect;
  RectF m_boundingBox;
  float m_rotationRate;
  // Actual unwrapped angular travel preserves fast authored falling spins.
  float m_presentationRotationTravel = 0.0f;
  float m_rotationFallThreshold;
  float m_rotationCap;
  List<PlantDropPiece> m_pieces;
  Json m_stemConfig;
  Json m_foliageConfig;
  Json m_saplingConfig;
  bool m_master;
  bool m_firstTick;
  NetElementBool m_spawnedDrops;
  bool m_spawnedDropEffects;
};

}

export module star.plant_drop;

export namespace Star {
  using ::Star::RenderCallback;
  using ::Star::PlantDrop;
}
