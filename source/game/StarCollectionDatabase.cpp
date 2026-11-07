import star.json;
import star.set;
import star.string;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.strong_typedef;
import star.bi_map;
import star.game_types;
import star.game_types;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.map;
import star.format;
import star.version;

import star.drawable;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.item;
import star.item_descriptor;
import star.item_recipe;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

import star.collection_database;
import star.monster_database;
import star.item_database;


namespace Star {


Collection::Collection() : name(), title(), type() {}

Collection::Collection(String const& name, CollectionType type, String const& title) : name(name), title(title), type(type) {}

Collectable::Collectable() : name(), order(), title(), description(), icon() {}

Collectable::Collectable(String const& name, int order, String const& title, String const& description, String const& icon)
  : name(name), order(order), title(title), description(description), icon(icon) {};

CollectionDatabase::CollectionDatabase(Root& root) : m_root(root) {}

std::expected<std::shared_ptr<CollectionDatabase>, StarException> CollectionDatabase::create(Root& root) {
  auto result = std::shared_ptr<CollectionDatabase>(new CollectionDatabase(root));
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto files = (*assets)->scanExtension("collection");
  if (!files)
    return std::unexpected(std::move(files.error()));
  auto queued = (*assets)->queueJsons(files->get());
  if (!queued)
    return std::unexpected(std::move(queued.error()));
  for (auto const& assetFile : files->get()) {
    auto const& file = assetFile.string();
    auto readFile = [&]() -> std::expected<void, StarException> {
      auto config = (*assets)->json(file);
      if (!config)
        return std::unexpected(std::move(config.error()));
      auto name = config->getString("name");
      if (!name)
        return std::unexpected(std::move(name.error()));
      auto title = config->getString("title", *name);
      if (!title)
        return std::unexpected(std::move(title.error()));
      auto typeName = config->getString("type", "generic");
      if (!typeName)
        return std::unexpected(std::move(typeName.error()));
      auto type = maybeEnumValue<CollectionTypeNames>(*typeName);
      if (!type)
        return std::unexpected(std::move(type.error()));
      if (!*type)
        return std::unexpected(CollectionDatabaseException(strf("Unknown collection type '{}'", *typeName)));
      Collection collection(*name, **type, *title);
      auto configs = config->get("collectables").and_then([](Json const& value) { return value.iterateObject(); });
      if (!configs)
        return std::unexpected(std::move(configs.error()));
      result->m_collectables[collection.name] = {};
      result->m_collectableConfigs[collection.name] = {};
      for (auto const& pair : *configs) {
        result->m_collectableConfigs[collection.name][pair.first] = pair.second;
        if (collection.type == CollectionType::Generic) {
          auto collectable = result->parseGenericCollectable(pair.first, pair.second);
          if (!collectable)
            return std::unexpected(std::move(collectable.error()));
          result->m_collectables[collection.name][pair.first] = std::move(*collectable);
        }
      }
      result->m_collections[collection.name] = std::move(collection);
      return {};
    };
    auto loaded = readFile();
    if (!loaded)
      return std::unexpected((*assets)->withAssetContext(std::move(loaded.error()), file, "Loading collection"));
  }
  return result;
}

List<Collection> CollectionDatabase::collections() const {
  return m_collections.values();
}

std::expected<Collection, StarException> CollectionDatabase::collection(String const& collectionName) const {
  if (auto result = m_collections.find_ref(collectionName))
    return result->get();
  return std::unexpected(CollectionDatabaseException(strf("Collection '{}' not found", collectionName)));
}

std::expected<List<Collectable>, StarException> CollectionDatabase::collectables(String const& collectionName) const {
  return collectables(collectionName, std::nullopt);
}

std::expected<List<Collectable>, StarException> CollectionDatabase::collectables(String const& collectionName, std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog) const {
  auto configs = m_collectableConfigs.find_ref(collectionName);
  if (!configs)
    return std::unexpected(CollectionDatabaseException(strf("Collection '{}' not found", collectionName)));
  List<Collectable> result;
  result.reserve(configs->get().size());
  for (auto const& entry : configs->get()) {
    auto value = collectable(collectionName, entry.first, itemCatalog);
    if (!value)
      return std::unexpected(std::move(value.error()));
    result.append(std::move(*value));
  }
  return result;
}

std::expected<Collectable, StarException> CollectionDatabase::collectable(String const& collectionName, String const& collectableName) const {
  return collectable(collectionName, collectableName, std::nullopt);
}

std::expected<Collectable, StarException> CollectionDatabase::collectable(String const& collectionName, String const& collectableName, std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog) const {
  auto configs = m_collectableConfigs.find_ref(collectionName);
  auto config = configs ? configs->get().find_ref(collectableName) : std::nullopt;
  auto definition = m_collections.find_ref(collectionName);
  if (!config || !definition)
    return std::unexpected(CollectionDatabaseException(strf("Collectable '{}' not found in collection '{}'", collectableName, collectionName)));
  {
    std::lock_guard locker(m_collectablesMutex);
    if (auto published = m_collectables.find_ref(collectionName))
      if (auto cached = published->get().find_ref(collectableName))
        return cached->get();
  }
  auto result = [&, &database = *this]() -> std::expected<Collectable, StarException> {
    switch (definition->get().type) {
      case CollectionType::Generic:
        return database.parseGenericCollectable(collectableName, config->get());
      case CollectionType::Monster:
        return database.parseMonsterCollectable(collectableName, config->get());
      case CollectionType::Item:
        return database.parseItemCollectable(collectableName, config->get(), itemCatalog);
    }
    return std::unexpected(CollectionDatabaseException("Invalid collection type"));
  }();
  if (!result) {
    if (result.error().is<MapException>())
      return std::unexpected(CollectionDatabaseException(strf("Collectable '{}' not found in collection '{}'", collectableName, collectionName), result.error()));
    return std::unexpected(std::move(result.error()));
  }
  std::lock_guard locker(m_collectablesMutex);
  auto& published = m_collectables[collectionName];
  if (auto cached = published.find_ref(collectableName))
    return cached->get();
  published[collectableName] = *result;
  return std::move(*result);
}

bool CollectionDatabase::hasCollectable(String const& collectionName, String const& collectableName) const {
  auto configs = m_collectableConfigs.find_ref(collectionName);
  return m_collections.contains(collectionName) && configs && configs->get().contains(collectableName);
}

std::expected<Collectable, StarException> CollectionDatabase::parseGenericCollectable(String const& name, Json const& config) const {
  auto order = config.getInt("order", 0);
  if (!order)
    return std::unexpected(std::move(order.error()));
  auto title = config.getString("title", "");
  if (!title)
    return std::unexpected(std::move(title.error()));
  auto description = config.getString("description", "");
  if (!description)
    return std::unexpected(std::move(description.error()));
  auto icon = config.getString("icon", "");
  if (!icon)
    return std::unexpected(std::move(icon.error()));
  return Collectable(name, *order, *title, *description, *icon);
}

std::expected<Collectable, StarException> CollectionDatabase::parseMonsterCollectable(String const& name, Json const& config) const {
  auto collectable = parseGenericCollectable(name, config);
  if (!collectable)
    return std::unexpected(std::move(collectable.error()));
  auto monsterType = config.getString("monsterType");
  if (!monsterType)
    return std::unexpected(std::move(monsterType.error()));
  auto database = m_root.monsterDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto variant = (*database)->monsterVariant(*monsterType, 0);
  if (!variant)
    return std::unexpected(std::move(variant.error()));
  collectable->title = variant->shortDescription.value_or("");
  collectable->description = variant->description.value_or("");
  return std::move(*collectable);
}

std::expected<Collectable, StarException> CollectionDatabase::parseItemCollectable(String const& name, Json const& config, std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog) const {
  auto collectable = parseGenericCollectable(name, config);
  if (!collectable)
    return std::unexpected(std::move(collectable.error()));
  std::shared_ptr<ItemDatabase const> catalogOwner;
  if (!itemCatalog) {
    auto catalog = m_root.itemDatabase();
    if (!catalog)
      return std::unexpected(std::move(catalog.error()));
    catalogOwner = std::move(*catalog);
    itemCatalog = std::cref(*catalogOwner);
  }
  auto itemName = config.getString("item");
  if (!itemName)
    return std::unexpected(std::move(itemName.error()));
  auto descriptor = ItemDescriptor::create(*itemName, 1);
  if (!descriptor)
    return std::unexpected(std::move(descriptor.error()));
  auto item = itemCatalog->get().itemShared(std::move(*descriptor));
  if (!item)
    return std::unexpected(std::move(item.error()));
  if (!*item)
    return std::unexpected(CollectionDatabaseException(strf("Collectable '{}' refers to an empty item", name)));
  collectable->title = (*item)->friendlyName();
  collectable->description = (*item)->description();
  auto configuredIcon = config.optString("icon");
  if (!configuredIcon)
    return std::unexpected(std::move(configuredIcon.error()));
  if (*configuredIcon) {
    collectable->icon = **configuredIcon;
  } else {
    auto inventoryIcon = (*item)->instanceValue("inventoryIcon", "");
    if (!inventoryIcon)
      return std::unexpected(std::move(inventoryIcon.error()));
    if (inventoryIcon->isType(Json::Type::String)) {
      auto itemConfig = itemCatalog->get().itemConfig((*item)->name(), JsonObject());
      if (!itemConfig)
        return std::unexpected(std::move(itemConfig.error()));
      auto icon = inventoryIcon->toString();
      if (!icon)
        return std::unexpected(std::move(icon.error()));
      collectable->icon = AssetPath::relativeTo(itemConfig->directory, *icon);
    }
  }
  return std::move(*collectable);
}

}
