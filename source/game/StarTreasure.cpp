import star.json;
import star.json_extra;
import star.id_map;
import star.casting;
import star.vector;
import star.data_stream;
import star.game_types;
import star.strong_typedef;
import star.bi_map;
import star.list;
import star.rpc_promise;
import star.set;
import star.string;
import star.color;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.format;
import star.version;
import star.random;
import star.weighted_pool;
import star.periodic;
import star.periodic_function;
import star.lua;
import star.audio;
import star.map;

import star.drawable;
import star.item;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
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
import star.tile_modification;
import star.lua_root;
import star.force_regions;
import star.world;
import star.lua_components;
import star.lua_animation_component;
import star.status_effect_entity;
import star.scripted_entity;
import star.chat_action;
import star.chatty_entity;
import star.wiring;
import star.wire_entity;
import star.inspectable_entity;
import star.animated_part_set;
import star.animation;
import star.particle;
import star.mixer;
import star.networked_animator;
import star.entity_rendering_types;
import star.entity_rendering;
import star.object;
import star.container_entity;
import star.item_recipe;
import star.container_object;


import star.object_database;
import star.item_database;
import star.treasure;

import star.item_bag;

import star.static_random;
import star.algorithm;
namespace Star {

std::expected<std::shared_ptr<TreasureDatabase>, StarException> TreasureDatabase::create(Root& root) {
  auto database = std::shared_ptr<TreasureDatabase>(new TreasureDatabase(root));
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto treasurePools = (*assets)->scanExtension("treasurepools");
  if (!treasurePools)
    return std::unexpected(std::move(treasurePools.error()));
  auto treasureChests = (*assets)->scanExtension("treasurechests");
  if (!treasureChests)
    return std::unexpected(std::move(treasureChests.error()));
  if (auto queued = (*assets)->queueJsons(treasurePools->get()); !queued)
    return std::unexpected(std::move(queued.error()));
  if (auto queued = (*assets)->queueJsons(treasureChests->get()); !queued)
    return std::unexpected(std::move(queued.error()));
  auto parseEntry = [](Json const& entry) -> std::expected<TreasureEntry, StarException> {
    auto hasPool = entry.contains("pool");
    if (!hasPool)
      return std::unexpected(std::move(hasPool.error()));
    if (*hasPool) {
      auto pool = entry.getString("pool");
      if (!pool)
        return std::unexpected(std::move(pool.error()));
      return TreasureEntry(std::move(*pool));
    }
    auto hasItem = entry.contains("item");
    if (!hasItem)
      return std::unexpected(std::move(hasItem.error()));
    if (*hasItem) {
      auto spec = entry.get("item");
      if (!spec)
        return std::unexpected(std::move(spec.error()));
      auto descriptor = ItemDescriptor::create(*spec);
      if (!descriptor)
        return std::unexpected(std::move(descriptor.error()));
      return TreasureEntry(std::move(*descriptor));
    }
    auto text = entry.repr();
    if (!text)
      return std::unexpected(std::move(text.error()));
    return std::unexpected(TreasureException::format("TreasurePool entry '{}' did not specify a valid 'item' or 'pool'", *text));
  };
  for (auto const& assetFile : treasurePools->get()) {
    auto const& file = assetFile.string();
    auto definitions = (*assets)->json(file).and_then([](Json const& value) { return value.iterateObject(); });
    if (!definitions)
      return std::unexpected(std::move(definitions.error()));
    for (auto const& pair : *definitions) {
      if (database->m_treasurePools.contains(pair.first))
        return std::unexpected(TreasureException::format("Duplicate TreasurePool config '{}' from file '{}'", pair.first, file));
      auto& treasurePool = database->m_treasurePools[pair.first];
      auto entries = pair.second.iterateArray();
      if (!entries)
        return std::unexpected(std::move(entries.error()));
      for (auto const& entry : *entries) {
        auto size = entry.size();
        if (!size)
          return std::unexpected(std::move(size.error()));
        if (*size != 2)
          return std::unexpected(TreasureException("Wrong size for TreasurePool entry, list must be 2"));
        auto startLevel = entry.getFloat(0);
        if (!startLevel)
          return std::unexpected(std::move(startLevel.error()));
        auto config = entry.get(1);
        if (!config)
          return std::unexpected(std::move(config.error()));
        ItemPool itemPool;
        auto fill = config->getArray("fill", {});
        if (!fill)
          return std::unexpected(std::move(fill.error()));
        for (auto const& fillEntry : *fill) {
          auto parsed = parseEntry(fillEntry);
          if (!parsed)
            return std::unexpected(std::move(parsed.error()));
          itemPool.fill.append(std::move(*parsed));
        }
        auto pool = config->getArray("pool", {});
        if (!pool)
          return std::unexpected(std::move(pool.error()));
        for (auto const& poolEntry : *pool) {
          auto hasWeight = poolEntry.contains("weight");
          if (!hasWeight)
            return std::unexpected(std::move(hasWeight.error()));
          if (!*hasWeight) {
            auto text = poolEntry.repr();
            if (!text)
              return std::unexpected(std::move(text.error()));
            return std::unexpected(TreasureException::format("TreasurePool entry '{}' did not specify a weight", *text));
          }
          auto weight = poolEntry.getFloat("weight");
          if (!weight)
            return std::unexpected(std::move(weight.error()));
          auto parsed = parseEntry(poolEntry);
          if (!parsed)
            return std::unexpected(std::move(parsed.error()));
          itemPool.pool.add(*weight, std::move(*parsed));
        }
        auto poolRounds = config->get("poolRounds", 1);
        if (!poolRounds)
          return std::unexpected(std::move(poolRounds.error()));
        if (poolRounds->canConvert(Json::Type::Float)) {
          auto rounds = poolRounds->toFloat();
          if (!rounds)
            return std::unexpected(std::move(rounds.error()));
          itemPool.poolRounds.add(1.0, static_cast<int>(*rounds));
        } else {
          auto rounds = poolRounds->iterateArray();
          if (!rounds)
            return std::unexpected(std::move(rounds.error()));
          for (auto const& round : *rounds) {
            auto weight = round.getDouble(0);
            if (!weight)
              return std::unexpected(std::move(weight.error()));
            auto count = round.getInt(1);
            if (!count)
              return std::unexpected(std::move(count.error()));
            itemPool.poolRounds.add(*weight, *count);
          }
        }
        auto variance = config->get("levelVariance", JsonArray{0, 0}).and_then([](Json const& value) { return jsonToVec2F(value); });
        if (!variance)
          return std::unexpected(std::move(variance.error()));
        itemPool.levelVariance = *variance;
        auto allowDuplication = config->getBool("allowDuplication", true);
        if (!allowDuplication)
          return std::unexpected(std::move(allowDuplication.error()));
        itemPool.allowDuplication = *allowDuplication;
        treasurePool.addPoint(*startLevel, std::move(itemPool));
      }
    }
  }
  for (auto const& assetFile : treasureChests->get()) {
    auto const& file = assetFile.string();
    auto definitions = (*assets)->json(file).and_then([](Json const& value) { return value.iterateObject(); });
    if (!definitions)
      return std::unexpected(std::move(definitions.error()));
    for (auto const& pair : *definitions) {
      if (database->m_treasureChestSets.contains(pair.first))
        return std::unexpected(TreasureException::format("Duplicate TreasureChestSet config '{}' from file '{}'", pair.first, file));
      auto& chestSet = database->m_treasureChestSets[pair.first];
      auto entries = pair.second.iterateArray();
      if (!entries)
        return std::unexpected(std::move(entries.error()));
      for (auto const& entry : *entries) {
        TreasureChest chest;
        auto containers = entry.get("containers").and_then([](Json const& value) { return jsonToStringList(value); });
        if (!containers)
          return std::unexpected(std::move(containers.error()));
        chest.containers = std::move(*containers);
        auto pool = entry.getString("treasurePool");
        if (!pool)
          return std::unexpected(std::move(pool.error()));
        chest.treasurePool = std::move(*pool);
        auto minimumLevel = entry.getFloat("minimumLevel", 0);
        if (!minimumLevel)
          return std::unexpected(std::move(minimumLevel.error()));
        chest.minimumLevel = *minimumLevel;
        if (!database->m_treasurePools.contains(chest.treasurePool))
          return std::unexpected(TreasureException::format("No such TreasurePool '{}' for TreasureChestSet named '{}' in file '{}'", chest.treasurePool, pair.first, file));
        chestSet.append(std::move(chest));
      }
    }
  }
  return database;
}

StringList TreasureDatabase::treasurePools() const {
  return m_treasurePools.keys();
}

bool TreasureDatabase::isTreasurePool(String const& treasurePool) const {
  return m_treasurePools.contains(treasurePool);
}

StringList TreasureDatabase::treasureChestSets() const {
  return m_treasureChestSets.keys();
}

bool TreasureDatabase::isTreasureChestSet(String const& treasurePool) const {
  return m_treasureChestSets.contains(treasurePool);
}

TreasureDatabase::ItemPool::ItemPool() : allowDuplication() {}

TreasureDatabase::TreasureChest::TreasureChest() : minimumLevel() {}

std::expected<List<std::shared_ptr<Item>>, StarException> TreasureDatabase::createTreasure(String const& treasurePool, float level) const {
  return createTreasure(treasurePool, level, m_root.random().randu64());
}

std::expected<List<std::shared_ptr<Item>>, StarException> TreasureDatabase::createTreasure(String const& treasurePool, float level, std::uint64_t seed) const {
  return createTreasure(treasurePool, level, seed, std::nullopt);
}

std::expected<List<std::shared_ptr<Item>>, StarException> TreasureDatabase::createTreasure(String const& treasurePool, float level, std::uint64_t seed, std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog) const {
  return createTreasure(treasurePool, level, seed, StringSet(), itemCatalog);
}

std::expected<List<std::shared_ptr<Item>>, StarException> TreasureDatabase::createTreasure(String const& treasurePool, float level, std::uint64_t seed, StringSet visitedPools, std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog) const {
  auto pool = m_treasurePools.maybeRef(treasurePool);
  if (!pool)
    return std::unexpected(TreasureException::format("Unknown treasure pool '{}'", treasurePool));
  if (!visitedPools.add(treasurePool))
    return std::unexpected(TreasureException::format("Loop detected in treasure pool generation - set '{}' already contains '{}'", visitedPools, treasurePool));
  std::shared_ptr<ItemDatabase const> catalogOwner;
  if (!itemCatalog) {
    auto catalog = m_root.itemDatabase();
    if (!catalog)
      return std::unexpected(std::move(catalog.error()));
    catalogOwner = std::move(*catalog);
    itemCatalog = std::cref(*catalogOwner);
  }
  List<std::shared_ptr<Item>> treasureItems;
  HashSet<ItemDescriptor> previousDescriptors;
  auto selected = pool->get().get(level);
  if (!selected)
    return std::unexpected(std::move(selected.error()));
  auto const& itemPool = selected->get();

  int mix = 0;
  for (auto const& fillEntry : itemPool.fill) {
    if (std::holds_alternative<String>(fillEntry)) {
      auto poolContents = createTreasure(std::get<String>(fillEntry), level, seed + ++mix, visitedPools, itemCatalog);
      if (!poolContents)
        return std::unexpected(std::move(poolContents.error()));
      for (auto const& item : *poolContents) {
        if (itemPool.allowDuplication || previousDescriptors.add(item->descriptor().singular()))
          treasureItems.append(item);
      }
    } else {
      float itemLevel = level + itemPool.levelVariance[0] + staticRandomFloat(seed, ++mix, "FillLevelVariance") * (itemPool.levelVariance[1] - itemPool.levelVariance[0]);
      auto fillItem = itemCatalog->get().item(std::get<ItemDescriptor>(fillEntry), itemLevel, seed + ++mix);
      if (!fillItem)
        return std::unexpected(std::move(fillItem.error()));
      if (itemPool.allowDuplication || previousDescriptors.add((*fillItem)->descriptor().singular()))
        treasureItems.append(std::move(*fillItem));
    }
  }

  if (!itemPool.pool.empty()) {
    int poolRounds = itemPool.poolRounds.select(staticRandomU64(seed, "TreasurePoolRounds"));

    for (int i = 0; i < poolRounds; ++i) {
      auto poolEntry = itemPool.pool.select(staticRandomU64(seed, i, "TreasureItem"));

      if (std::holds_alternative<String>(poolEntry)) {
        auto poolContents = createTreasure(std::get<String>(poolEntry), level, staticRandomU64(seed, i, "TreasureSeedRecursion"), visitedPools, itemCatalog);
        if (!poolContents)
          return std::unexpected(std::move(poolContents.error()));
        for (auto const& item : *poolContents) {
          if (itemPool.allowDuplication || previousDescriptors.add(item->descriptor().singular()))
            treasureItems.append(item);
        }
      } else {
        float itemLevel = level + itemPool.levelVariance[0] + staticRandomFloat(staticRandomU64(seed, i, "TreasureLevelSeedMixer"), "PoolLevelVariance") * (itemPool.levelVariance[1] - itemPool.levelVariance[0]);
        auto roundItem = std::get<ItemDescriptor>(poolEntry);
        if (itemPool.allowDuplication || previousDescriptors.add(roundItem.singular())) {
          auto item = itemCatalog->get().item(roundItem, itemLevel, seed + ++mix);
          if (!item)
            return std::unexpected(std::move(item.error()));
          treasureItems.append(std::move(*item));
        }
      }
    }
  }

  return treasureItems;
}

std::expected<List<std::shared_ptr<Item>>, StarException> TreasureDatabase::fillWithTreasure(
    std::shared_ptr<ItemBag> const& itemBag, String const& treasurePool, float level) const {
  return fillWithTreasure(itemBag, treasurePool, level, m_root.random().randu64());
}

std::expected<List<std::shared_ptr<Item>>, StarException> TreasureDatabase::fillWithTreasure(
    std::shared_ptr<ItemBag> const& itemBag, String const& treasurePool, float level, std::uint64_t seed) const {
  auto treasure = createTreasure(treasurePool, level, seed);
  if (!treasure)
    return std::unexpected(std::move(treasure.error()));
  List<std::shared_ptr<Item>> overflowItems;
  for (auto const& item : *treasure) {
    auto overflow = itemBag->addItems(item);
    if (!overflow)
      return std::unexpected(std::move(overflow.error()));
    if (*overflow)
      overflowItems.append(std::move(*overflow));
  }
  return overflowItems;
}

std::expected<std::shared_ptr<ContainerObject>, StarException> TreasureDatabase::createTreasureChest(World& world, String const& treasureChestSet, Vec2I const& position, Direction direction) const {
  return createTreasureChest(world, treasureChestSet, position, direction, m_root.random().randu64());
}

std::expected<std::shared_ptr<ContainerObject>, StarException> TreasureDatabase::createTreasureChest(World& world, String const& treasureChestSet, Vec2I const& position, Direction direction, std::uint64_t seed) const {
  auto objectDatabase = m_root.objectDatabase();
  if (!objectDatabase)
    return std::unexpected(std::move(objectDatabase.error()));
  auto set = m_treasureChestSets.maybeRef(treasureChestSet);
  if (!set)
    return std::unexpected(StarException::format("Unknown treasure chest set '{}'", treasureChestSet));
  auto level = world.threatLevel();
  if (!level)
    return std::unexpected(std::move(level.error()));
  auto boxSet = set->get();
  eraseWhere(boxSet, [&](TreasureChest const& chest) { return *level < chest.minimumLevel; });
  if (boxSet.empty())
    return std::shared_ptr<ContainerObject>();
  auto chest = staticRandomFrom(boxSet, seed, "TreasureChest");
  if (!chest)
    return std::unexpected(std::move(chest.error()));
  auto containerName = staticRandomFrom(chest->get().containers, seed, "ContainerName");
  if (!containerName)
    return std::unexpected(std::move(containerName.error()));
  auto parameters = JsonObject{{"treasurePools", JsonArray{chest->get().treasurePool}}, {"treasureSeed", seed}};
  auto object = (*objectDatabase)->createForPlacement(world, containerName->get(), position, direction, parameters);
  if (!object)
    return std::unexpected(std::move(object.error()));
  if (!*object)
    return std::shared_ptr<ContainerObject>();
  return convert<ContainerObject>(*object);
}

}
