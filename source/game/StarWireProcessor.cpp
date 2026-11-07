import star.format;
import star.xx_hash;
import star.json;
import star.id_map;
import star.rect;
import star.map;
import star.set;
import star.spatial_hash_2d;
import star.casting;
import star.vector;
import star.strong_typedef;
import star.bi_map;
import star.game_types;
import star.data_stream;
import star.list;
import star.logging;
import star.btree;
import star.ordered_map;
import star.lru_cache;
import star.data_stream_devices;
import star.thread;
import star.btree_database;
import star.ordered_set;
import star.version;
import star.rpc_promise;
import star.random;
import star.perlin;
import star.weighted_pool;
import std;
import star.worker_pool;

import star.sector_array_2d;
import star.color;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;

import star.damage_types;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.world_geometry;
import star.wiring;
import star.wire_processor;
import star.root;
import star.liquid_types;
import star.tile_sector_array;
import star.animation;
import star.particle;
import star.weather_types;
import star.sky_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.inspectable_entity;
import star.plant;
import star.plant_database;
import star.biome_placement;
import star.lua_root;
import star.versioning_database;
import star.world_storage;
import star.tile_damage;
import star.interaction_types;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.wire_entity;

import star.entity_map;

namespace Star {

WireProcessor::WireProcessor(std::shared_ptr<WorldStorage> worldStorage) { m_worldStorage = std::move(worldStorage); }

std::expected<void, StarException> WireProcessor::process() {
  // First, populate all the working entities that are already live
  auto populated = m_worldStorage->entityMap()->forAllEntities([&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
    if (auto wireEntity = asRef<WireEntity>(*entity)) return populateWorking(wireEntity->get());
    return {};
  });
  if (!populated) return std::unexpected(std::move(populated.error()));

  // Then, scan the network of each entity in the working set.  This may, as a
  // side effect, load further unconnected wire entities. Because our policy is
  // to try as hard as possible to make sure that the entire wire entity
  // network to be loaded at once or not at all, we need to make sure that each
  // new disconnected entity also has its network loaded and so on.  Thus, if
  // the working entities size changes during scanning, simply scan the whole
  // thing again until the size stops changing.
  while (true) {
    std::size_t oldWorkingSize = m_workingWireEntities.size();
    for (auto const& p : m_workingWireEntities.keys()) {
      auto state = m_workingWireEntities.get(p);
      if (!state) return std::unexpected(std::move(state.error()));
      if (!state->get().networkLoaded) {
        if (auto r = loadNetwork(p); !r) return std::unexpected(std::move(r.error()));
      }
    }
    if (m_workingWireEntities.size() == oldWorkingSize)
      break;
  }

  for (auto const& p : m_workingWireEntities) {
    if (auto r = p.second.wireEntity.get().evaluate(*this); !r) return std::unexpected(std::move(r.error()));
  }

  m_workingWireEntities.clear();
  return {};
}

std::expected<bool, StarException> WireProcessor::readInputConnection(WireConnection const& connection) {
  if (auto wes = m_workingWireEntities.find_ref(connection.entityLocation)) {
    return wes->get().outputStates.get(connection.nodeIndex);
  }
  return false;
}

std::expected<void, StarException> WireProcessor::populateWorking(WireEntity& wireEntity) {
  auto p = m_workingWireEntities.insert(
    wireEntity.tilePosition(),
    WireEntityState{.wireEntity = std::ref(wireEntity), .outputStates = {}, .networkLoaded = false});
  if (!p.second) {
    if (p.first->second.wireEntity.get().entityId() != wireEntity.entityId()) {
      if (auto r = wireEntity.root().logger().debug("Multiple wire entities share tile position: {}", wireEntity.position()); !r) return std::unexpected(std::move(r.error()));
    }
    return {};
  }
  auto& wes = p.first->second;
  std::size_t outputNodeCount = wes.wireEntity.get().nodeCount(WireDirection::Output);
  wes.outputStates.resize(outputNodeCount);
  for (std::size_t i = 0; i < outputNodeCount; ++i) {
    auto state = wes.wireEntity.get().nodeState({.direction = WireDirection::Output, .nodeIndex = i});
    if (!state) return std::unexpected(std::move(state.error()));
    wes.outputStates[i] = *state;
  }
  return {};
}

std::expected<void, StarException> WireProcessor::loadNetwork(Vec2I tilePosition) {
  HashSet<WorldStorage::Sector> networkSectors;
  std::optional<float> highestTtl;

  // Recursively load a given WireEntity at the given position.  Returns true
  // if that wire entity was found.
  // TODO: This is depth first recursive, because that is the simplest thing,
  // but if this causes issues with recursion depth it can be changed.
  auto doLoad = [&](this auto const& self, Vec2I const& pos) -> std::expected<bool, StarException> {
    auto sector = m_worldStorage->sectorForPosition(pos);
    if (!sector)
      return false;

    if (m_worldStorage->sectorLoadLevel(*sector) == SectorLoadLevel::Loaded) {
      auto ttl = *m_worldStorage->sectorTimeToLive(*sector);
      if (highestTtl)
        highestTtl = std::max(*highestTtl, ttl);
      else
        highestTtl = ttl;
    } else {
      if (auto r = m_worldStorage->loadSector(*sector); !r) return std::unexpected(std::move(r.error()));
      auto region = m_worldStorage->regionForSector(*sector);
      if (!region) return std::unexpected(StarException("Wire sector has no region"));
      auto populated = m_worldStorage->entityMap()->forEachEntity(RectF(*region), [&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
        if (auto wireEntity = asRef<WireEntity>(*entity)) return populateWorking(wireEntity->get());
        return {};
      });
      if (!populated) return std::unexpected(std::move(populated.error()));
    }

    auto wes = m_workingWireEntities.find_ref(pos);
    if (!wes)
      return false;
    if (wes->get().networkLoaded)
      return true;

    wes->get().networkLoaded = true;
    networkSectors.add(*sector);

    // Recursively descend into all the inbound and outbound nodes, and if we
    // ever cannot load the wire entity for a connection, go ahead and remove
    // the connection.
    std::size_t inboundNodeCount = wes->get().wireEntity.get().nodeCount(WireDirection::Input);
    for (std::size_t i = 0; i < inboundNodeCount; ++i) {
      auto connections =
        wes->get().wireEntity.get().connectionsForNode({.direction = WireDirection::Input, .nodeIndex = i});
      if (!connections) return std::unexpected(std::move(connections.error()));
      for (auto const& connection : *connections) {
        auto loaded = self(connection.entityLocation);
        if (!loaded) return std::unexpected(std::move(loaded.error()));
        if (!*loaded) {
          if (auto r = wes->get().wireEntity.get().removeNodeConnection(
                {.direction = WireDirection::Input, .nodeIndex = i}, connection);
              !r)
            return std::unexpected(std::move(r.error()));
        }
      }
    }
    std::size_t outboundNodeCount = wes->get().wireEntity.get().nodeCount(WireDirection::Output);
    for (std::size_t i = 0; i < outboundNodeCount; ++i) {
      auto connections =
        wes->get().wireEntity.get().connectionsForNode({.direction = WireDirection::Output, .nodeIndex = i});
      if (!connections) return std::unexpected(std::move(connections.error()));
      for (auto const& connection : *connections) {
        auto loaded = self(connection.entityLocation);
        if (!loaded) return std::unexpected(std::move(loaded.error()));
        if (!*loaded) {
          if (auto r = wes->get().wireEntity.get().removeNodeConnection(
                {.direction = WireDirection::Output, .nodeIndex = i}, connection);
              !r)
            return std::unexpected(std::move(r.error()));
        }
      }
    }
    return true;
  };

  if (auto r = doLoad(tilePosition); !r) return std::unexpected(std::move(r.error()));

  // Set the sector ttl for the entire network to be equal to the highest
  // entry, so that the entire network either lives or dies together, but
  // without artificially extending the lifetime of the network.
  if (highestTtl) {
    for (auto const& sector : networkSectors)
      m_worldStorage->setSectorTimeToLive(sector, *highestTtl);
  }
  return {};
}

}
