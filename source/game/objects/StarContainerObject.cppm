module;
import star.format;
import star.game_timers;
import std;
import star.type_identity;
import star.memory;
import star.json;
import star.id_map;
import star.random;
import star.weighted_pool;
import star.casting;
import star.strong_typedef;
import star.list;
import star.rpc_promise;
import star.data_stream;
import star.bi_map;
import star.game_types;
import star.periodic;
import star.periodic_function;
import star.set;
import star.color;
import star.lua;
import star.audio;
import star.thread;
import star.map;
import star.string;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ordered_map;

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
import star.object;
import star.container_entity;
import star.item_recipe;

import star.net_element_basic_fields;
import star.net_element_float_fields;

namespace Star {
class Root;

class ContainerObject : public Object, public virtual ContainerEntity {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<ContainerObject, Object, ContainerEntity>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<ContainerObject>();
  }

  [[nodiscard]] static std::expected<std::shared_ptr<ContainerObject>, StarException> create(Root& rootContext, std::shared_ptr<ObjectConfig const> config, Json const& parameters);

  [[nodiscard]] std::expected<void, StarException> init(World& world, EntityId entityId, EntityMode mode) override;

  [[nodiscard]] std::expected<void, StarException> update(float dt, std::uint64_t currentStep) override;
  [[nodiscard]] std::expected<void, StarException> collectRenderEvents(RenderCallback& renderCallback) override;

  [[nodiscard]] std::expected<void, StarException> destroy(std::optional<std::reference_wrapper<RenderCallback>> renderCallback) override;
  [[nodiscard]] std::expected<InteractAction, StarException> interact(InteractRequest const& request) override;

  [[nodiscard]] std::expected<std::optional<ChainableJsonMessageResponse>, StarException> receiveMessage(ConnectionId sendingConnection, String const& message, JsonArray const& args) override;

  [[nodiscard]] std::expected<Json, StarException> containerGuiConfig() const override;
  [[nodiscard]] std::expected<String, StarException> containerDescription() const override;
  [[nodiscard]] std::expected<String, StarException> containerSubTitle() const override;
  [[nodiscard]] std::expected<ItemDescriptor, StarException> iconItem() const override;

  std::shared_ptr<ItemBag const> itemBag() const override;

  [[nodiscard]] std::expected<void, StarException> containerOpen() override;
  [[nodiscard]] std::expected<void, StarException> containerClose() override;

  [[nodiscard]] std::expected<void, StarException> startCrafting() override;
  [[nodiscard]] std::expected<void, StarException> stopCrafting() override;
  bool isCrafting() const override;
  float craftingProgress() const override;

  [[nodiscard]] std::expected<void, StarException> burnContainerContents() override;

  [[nodiscard]] std::expected<RpcPromise<std::shared_ptr<Item>>, StarException> addItems(std::shared_ptr<Item> const& items) override;
  [[nodiscard]] std::expected<RpcPromise<std::shared_ptr<Item>>, StarException> putItems(std::size_t slot, std::shared_ptr<Item> const& items) override;
  [[nodiscard]] std::expected<RpcPromise<std::shared_ptr<Item>>, StarException> takeItems(std::size_t slot, std::size_t count = NPos) override;
  [[nodiscard]] std::expected<RpcPromise<std::shared_ptr<Item>>, StarException> swapItems(std::size_t slot, std::shared_ptr<Item> const& items, bool tryCombine = true) override;
  [[nodiscard]] std::expected<RpcPromise<std::shared_ptr<Item>>, StarException> applyAugment(std::size_t slot, std::shared_ptr<Item> const& augment) override;
  [[nodiscard]] std::expected<RpcPromise<bool>, StarException> consumeItems(ItemDescriptor const& descriptor) override;
  [[nodiscard]] std::expected<RpcPromise<bool>, StarException> consumeItems(std::size_t slot, std::size_t count) override;
  [[nodiscard]] std::expected<RpcPromise<List<std::shared_ptr<Item>>>, StarException> clearContainer() override;

protected:
  [[nodiscard]] std::expected<void, StarException> getNetStates(bool initial) override;
  [[nodiscard]] std::expected<void, StarException> setNetStates() override;

  [[nodiscard]] std::expected<void, StarException> readStoredData(Json const& diskStore) override;
  [[nodiscard]] std::expected<Json, StarException> writeStoredData() const override;

private:
  explicit ContainerObject(Root& rootContext);
  typedef std::function<void(ContainerObject&)> ContainerCallback;

  [[nodiscard]] std::expected<ItemRecipe, StarException> recipeForMaterials(List<std::shared_ptr<Item>> const& inputItems);
  [[nodiscard]] std::expected<void, StarException> tickCrafting(float dt);

  [[nodiscard]] std::expected<std::shared_ptr<Item>, StarException> doAddItems(std::shared_ptr<Item> const& items);
  [[nodiscard]] std::expected<std::shared_ptr<Item>, StarException> doPutItems(std::size_t slot, std::shared_ptr<Item> const& items);
  [[nodiscard]] std::expected<std::shared_ptr<Item>, StarException> doTakeItems(std::size_t slot, std::size_t count = NPos);
  [[nodiscard]] std::expected<std::shared_ptr<Item>, StarException> doSwapItems(std::size_t slot, std::shared_ptr<Item> const& items, bool tryCombine = true);
  [[nodiscard]] std::expected<std::shared_ptr<Item>, StarException> doApplyAugment(std::size_t slot, std::shared_ptr<Item> const& augment);
  [[nodiscard]] std::expected<bool, StarException> doConsumeItems(ItemDescriptor const& descriptor);
  [[nodiscard]] std::expected<bool, StarException> doConsumeItems(std::size_t slot, std::size_t count);
  [[nodiscard]] std::expected<List<std::shared_ptr<Item>>, StarException> doClearContainer();


  void itemsUpdated();

  NetElementInt m_opened;

  NetElementBool m_crafting;
  NetElementFloat m_craftingProgress;

  std::shared_ptr<ItemBag> m_items;
  NetElementBytes m_itemsNetState;

  // master only

  bool m_initialized;
  int m_count;
  int m_currentState;
  std::int64_t m_animationFrameCooldown;
  std::int64_t m_autoCloseCooldown;

  ItemRecipe m_goalRecipe;

  bool m_itemsUpdated;
  bool m_runUpdatedCallback;

  ContainerCallback m_containerCallback;

  EpochTimer m_ageItemsTimer;

  List<std::shared_ptr<Item>> m_lostItems;
};

}

export module star.container_object;

export namespace Star {
  using ::Star::ContainerObject;
}
