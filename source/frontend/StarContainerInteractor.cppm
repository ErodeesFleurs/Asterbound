module;
import std;
import star.format;
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
import star.container_entity;

namespace Star {

using ContainerResult = List<std::shared_ptr<Item>>;

class ContainerInteractor {
public:
  ContainerInteractor() = default;

  [[nodiscard]] std::expected<void, StarException> openContainer(std::shared_ptr<ContainerEntity> containerEntity);
  [[nodiscard]] std::expected<void, StarException> closeContainer();

  bool containerOpen() const;

  // Returns NullEntityId if no container is open
  EntityId openContainerId() const;

  // This does not perform any checks; make sure to check if it is valid if you use it!
  std::shared_ptr<ContainerEntity> const& openContainer() const;

  [[nodiscard]] std::expected<List<ContainerResult>, StarException> pullContainerResults();

  [[nodiscard]] std::expected<void, StarException> swapInContainer(std::size_t slot, std::shared_ptr<Item> const& items);
  [[nodiscard]] std::expected<void, StarException> addToContainer(std::shared_ptr<Item> const& items);
  [[nodiscard]] std::expected<void, StarException> takeFromContainerSlot(std::size_t slot, std::size_t count);
  [[nodiscard]] std::expected<void, StarException> applyAugmentInContainer(std::size_t slot, std::shared_ptr<Item> const& augment);

  [[nodiscard]] std::expected<void, StarException> startCraftingInContainer();
  [[nodiscard]] std::expected<void, StarException> stopCraftingInContainer();
  [[nodiscard]] std::expected<void, StarException> burnContainer();
  [[nodiscard]] std::expected<void, StarException> clearContainer();

private:
  [[nodiscard]] static std::expected<ContainerResult, StarException> resultFromItem(std::shared_ptr<Item> const& items);

  mutable std::shared_ptr<ContainerEntity> m_openContainer;
  List<RpcPromise<ContainerResult>> m_pendingResults;
};

}

export module star.container_interactor;

export namespace Star {
  using ::Star::ContainerInteractor;
  using ::Star::ContainerResult;
}
