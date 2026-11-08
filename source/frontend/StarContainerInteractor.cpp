import std;
import star.optional;
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


import star.container_interactor;
import star.root;

import star.algorithm;
namespace Star {

std::expected<void, StarException> ContainerInteractor::openContainer(std::shared_ptr<ContainerEntity> containerEntity) {
  if (m_openContainer && m_openContainer->inWorld()) {
    auto closed = m_openContainer->containerClose();
    if (!closed)
      return std::unexpected(std::move(closed.error()));
  }
  m_openContainer = std::move(containerEntity);
  if (m_openContainer) {
    if constexpr (Star::DebugEnabled) {
      Star::star_assert_failed((m_openContainer->inWorld()));
    }
    auto opened = m_openContainer->containerOpen();
    if (!opened) {
      m_openContainer.reset();
      return std::unexpected(std::move(opened.error()));
    }
  }
  return {};
}

std::expected<void, StarException> ContainerInteractor::closeContainer() {
  return openContainer({});
}

bool ContainerInteractor::containerOpen() const {
  return openContainerId() != NullEntityId;
}

EntityId ContainerInteractor::openContainerId() const {
  if (m_openContainer && !m_openContainer->inWorld())
    m_openContainer = {};

  if (m_openContainer)
    return m_openContainer->entityId();

  return NullEntityId;
}

// Make sure to check if this is valid when you use it!
std::shared_ptr<ContainerEntity> const& ContainerInteractor::openContainer() const {
  if (m_openContainer && !m_openContainer->inWorld())
    m_openContainer = {};

  return m_openContainer;
}

std::expected<List<ContainerResult>, StarException> ContainerInteractor::pullContainerResults() {
  List<ContainerResult> results;
  for (auto it = m_pendingResults.begin(); it != m_pendingResults.end();) {
    auto result = it->result();
    if (!result)
      return std::unexpected(std::move(result.error()));
    auto finished = it->finished();
    if (!finished)
      return std::unexpected(std::move(finished.error()));
    if (*result)
      results.push_back(std::move(**result));
    if (*finished)
      it = m_pendingResults.erase(it);
    else
      ++it;
  }
  return results;
}

std::expected<void, StarException> ContainerInteractor::swapInContainer(std::size_t slot, std::shared_ptr<Item> const& items) {
  if (auto container = openContainer()) {
    auto promise = container->swapItems(slot, items);
    if (!promise)
      return std::unexpected(std::move(promise.error()));
    auto wrapped = promise->wrap(resultFromItem);
    if (!wrapped)
      return std::unexpected(std::move(wrapped.error()));
    m_pendingResults.push_back(std::move(*wrapped));
  }
  return {};
}

std::expected<void, StarException> ContainerInteractor::addToContainer(std::shared_ptr<Item> const& items) {
  if (auto container = openContainer()) {
    auto promise = container->addItems(items);
    if (!promise)
      return std::unexpected(std::move(promise.error()));
    auto wrapped = promise->wrap(resultFromItem);
    if (!wrapped)
      return std::unexpected(std::move(wrapped.error()));
    m_pendingResults.push_back(std::move(*wrapped));
  }
  return {};
}

std::expected<void, StarException> ContainerInteractor::takeFromContainerSlot(std::size_t slot, std::size_t count) {
  if (auto container = openContainer()) {
    auto promise = container->takeItems(slot, count);
    if (!promise)
      return std::unexpected(std::move(promise.error()));
    auto wrapped = promise->wrap(resultFromItem);
    if (!wrapped)
      return std::unexpected(std::move(wrapped.error()));
    m_pendingResults.push_back(std::move(*wrapped));
  }
  return {};
}

std::expected<void, StarException> ContainerInteractor::applyAugmentInContainer(std::size_t slot, std::shared_ptr<Item> const& augment) {
  if (auto container = openContainer()) {
    auto promise = container->applyAugment(slot, augment);
    if (!promise)
      return std::unexpected(std::move(promise.error()));
    auto wrapped = promise->wrap(resultFromItem);
    if (!wrapped)
      return std::unexpected(std::move(wrapped.error()));
    m_pendingResults.push_back(std::move(*wrapped));
  }
  return {};
}

std::expected<void, StarException> ContainerInteractor::startCraftingInContainer() {
  if (auto container = openContainer())
    return container->startCrafting();
  return {};
}

std::expected<void, StarException> ContainerInteractor::stopCraftingInContainer() {
  if (auto container = openContainer())
    return container->stopCrafting();
  return {};
}

std::expected<void, StarException> ContainerInteractor::burnContainer() {
  if (auto container = openContainer())
    return container->burnContainerContents();
  return {};
}

std::expected<void, StarException> ContainerInteractor::clearContainer() {
  if (auto container = openContainer()) {
    auto promise = container->clearContainer();
    if (!promise)
      return std::unexpected(std::move(promise.error()));
    m_pendingResults.push_back(std::move(*promise));
  }
  return {};
}

std::expected<ContainerResult, StarException> ContainerInteractor::resultFromItem(std::shared_ptr<Item> const& item) {
  if (item)
    return ContainerResult{item};
  return ContainerResult{};
}

}
