module;
import std;
import star.format;
import star.type_identity;
import star.memory;
import star.string;
import star.json;
import star.id_map;
import star.game_types;
import star.casting;
import star.data_stream;
import star.strong_typedef;
import star.bi_map;
import star.list;
import star.rpc_promise;


import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;

import star.light_source;

import star.entity;
import star.tile_damage;

import star.interaction_types;

import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;

import star.interactive_entity;
import star.collision_block;

import star.tile_entity;

namespace Star {

class Item;
class ItemBag;

// All container methods may be called on both master and slave entities.
class ContainerEntity : public virtual TileEntity {
public:
  void const* query_type(TypeKey key) const noexcept override {
    return query_type_of<ContainerEntity, TileEntity>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<ContainerEntity>(); }

  std::size_t containerSize() const;
  List<std::shared_ptr<Item>> containerItems() const;

  [[nodiscard]] virtual std::expected<Json, StarException> containerGuiConfig() const = 0;
  [[nodiscard]] virtual std::expected<String, StarException> containerDescription() const = 0;
  [[nodiscard]] virtual std::expected<String, StarException> containerSubTitle() const = 0;
  [[nodiscard]] virtual std::expected<ItemDescriptor, StarException> iconItem() const = 0;

  virtual std::shared_ptr<ItemBag const> itemBag() const = 0;

  [[nodiscard]] virtual std::expected<void, StarException> containerOpen() = 0;
  [[nodiscard]] virtual std::expected<void, StarException> containerClose() = 0;

  [[nodiscard]] virtual std::expected<void, StarException> startCrafting() = 0;
  [[nodiscard]] virtual std::expected<void, StarException> stopCrafting() = 0;
  virtual bool isCrafting() const = 0;
  virtual float craftingProgress() const = 0;

  [[nodiscard]] virtual std::expected<void, StarException> burnContainerContents() = 0;

  [[nodiscard]] virtual std::expected<RpcPromise<std::shared_ptr<Item>>, StarException> addItems(std::shared_ptr<Item> const& items) = 0;
  [[nodiscard]] virtual std::expected<RpcPromise<std::shared_ptr<Item>>, StarException> putItems(std::size_t slot, std::shared_ptr<Item> const& items) = 0;
  [[nodiscard]] virtual std::expected<RpcPromise<std::shared_ptr<Item>>, StarException> takeItems(std::size_t slot, std::size_t count = NPos) = 0;
  [[nodiscard]] virtual std::expected<RpcPromise<std::shared_ptr<Item>>, StarException> swapItems(std::size_t slot, std::shared_ptr<Item> const& items, bool tryCombine = true) = 0;
  [[nodiscard]] virtual std::expected<RpcPromise<std::shared_ptr<Item>>, StarException> applyAugment(std::size_t slot, std::shared_ptr<Item> const& augment) = 0;
  [[nodiscard]] virtual std::expected<RpcPromise<bool>, StarException> consumeItems(ItemDescriptor const& descriptor) = 0;
  [[nodiscard]] virtual std::expected<RpcPromise<bool>, StarException> consumeItems(std::size_t slot, std::size_t count) = 0;
  [[nodiscard]] virtual std::expected<RpcPromise<List<std::shared_ptr<Item>>>, StarException> clearContainer() = 0;
};

}

export module star.container_entity;

export namespace Star {
  using ::Star::Item;
  using ::Star::ItemBag;
  using ::Star::ContainerEntity;
}
