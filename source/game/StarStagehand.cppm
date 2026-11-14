module;
import star.json;
import star.net_compatibility;
import star.byte_array;
import star.id_map;
import star.casting;
import star.vector;
import star.data_stream;
import star.game_types;
import star.strong_typedef;
import star.bi_map;
import star.lua;
import star.ordered_map;
import star.rect;
import star.thread;
import star.io_device;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.format;
import star.type_identity;
import star.string;
import star.version;

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.lua_components;
import star.scripted_entity;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

import star.net_element_basic_fields;
import star.net_element_float_fields;
import star.net_element_system;

namespace Star {
class Root;

class BehaviorState;

class Stagehand : public virtual ScriptedEntity {
public:
  void const* query_type(TypeKey key) const noexcept override {
    return query_type_of<Stagehand, ScriptedEntity>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<Stagehand>(); }

  [[nodiscard]] static std::expected<std::shared_ptr<Stagehand>, StarException> create(Root& rootContext, Json const& config);
  [[nodiscard]] static std::expected<std::shared_ptr<Stagehand>, StarException> create(Root& rootContext, ByteArray const& netStore, NetCompatibilityRules rules = {});

  [[nodiscard]] std::expected<Json, StarException> diskStore() const;
  [[nodiscard]] std::expected<ByteArray, StarException> netStore(NetCompatibilityRules rules = {});

  [[nodiscard]] std::expected<void, StarException> init(World& world, EntityId entityId, EntityMode mode) override;
  [[nodiscard]] std::expected<void, StarException> uninit() override;

  EntityType entityType() const override;

  void setPosition(Vec2F const& position);

  Vec2F position() const override;

  RectF metaBoundBox() const override;

  [[nodiscard]] std::expected<std::pair<ByteArray, std::uint64_t>, StarException> writeNetState(std::uint64_t fromVersion = 0, NetCompatibilityRules rules = {}) override;
  [[nodiscard]] std::expected<void, StarException> readNetState(ByteArray data, float interpolationTime = 0.0f, NetCompatibilityRules rules = {}) override;

  [[nodiscard]] std::expected<String, StarException> name() const override;

  [[nodiscard]] std::expected<void, StarException> update(float dt, std::uint64_t currentStep) override;

  [[nodiscard]] std::expected<bool, StarException> shouldDestroy() const override;
  
  ClientEntityMode clientEntityMode() const override;

  [[nodiscard]] std::expected<std::optional<LuaValue>, StarException> callScript(String const& func, LuaVariadic<LuaValue> const& args) override;
  [[nodiscard]] std::expected<std::optional<LuaValue>, StarException> evalScript(String const& code) override;

  [[nodiscard]] std::expected<String, StarException> typeName() const;

  [[nodiscard]] std::expected<Json, StarException> configValue(String const& name, Json const& def = Json()) const;

  [[nodiscard]] std::expected<std::optional<ChainableJsonMessageResponse>, StarException> receiveMessage(ConnectionId sendingConnection, String const& message, JsonArray const& args) override;

  using Entity::setUniqueId;

private:
  Stagehand(Root& rootContext);

  [[nodiscard]] std::expected<void, StarException> readConfig(Json config);

  [[nodiscard]] std::expected<LuaCallbacks, StarException> makeStagehandCallbacks();

  Json m_config;

  RectF m_boundBox;

  bool m_dead = false;
      
  ClientEntityMode m_clientEntityMode;

  NetElementTopGroup m_netGroup;

  NetElementFloat m_xPosition;
  NetElementFloat m_yPosition;

  NetElementData<std::optional<String>> m_uniqueIdNetState;

  bool m_scripted = false;
  List<std::shared_ptr<BehaviorState>> m_behaviors;
  LuaStoredEntityScript
      m_scriptComponent;
};

}

export module star.stagehand;

export namespace Star {
  using ::Star::Stagehand;
  using ::Star::BehaviorState;
}
