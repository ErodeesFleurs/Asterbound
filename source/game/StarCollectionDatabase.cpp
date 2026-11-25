import std;
import star.asset_path;
import star.assets;
import star.bi_map;
import star.collection_database;
import star.format;
import star.item;
import star.item_database;
import star.item_descriptor;
import star.json;
import star.list;
import star.map;
import star.monster_database;
import star.root;
import star.string;

namespace Star {
namespace {
std::expected<Collectable, StarException> parse_generic(String const& name, Json const& config);
}

std::expected<std::shared_ptr<CollectionDatabase>, StarException> CollectionDatabase::create(Root& root) {
  auto result = std::make_shared<CollectionDatabase>(ConstructionKey{}, root);
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto files = (*assets)->scan_extension("collection");
  if (!files)
    return std::unexpected(std::move(files.error()));
  auto queued = (*assets)->queue_jsons(files->get());
  if (!queued)
    return std::unexpected(std::move(queued.error()));
  for (auto const& asset_file : files->get()) {
    auto const& file = asset_file.string();
    auto read_file = [&]() -> std::expected<void, StarException> {
      auto config = (*assets)->json(file);
      if (!config)
        return std::unexpected(std::move(config.error()));
      auto name = config->get_string("name");
      if (!name)
        return std::unexpected(std::move(name.error()));
      auto title = config->get_string("title", *name);
      if (!title)
        return std::unexpected(std::move(title.error()));
      auto type_name = config->get_string("type", "generic");
      if (!type_name)
        return std::unexpected(std::move(type_name.error()));
      auto type = maybe_enum_value<collection_type_names>(*type_name);
      if (!type)
        return std::unexpected(std::move(type.error()));
      if (!*type)
        return std::unexpected(CollectionDatabaseException(Star::format("Unknown collection type '{}'", *type_name)));
      Group group{Collection{std::move(*name), std::move(*title), **type}, {}};
      auto configs = config->get("collectables").and_then([](Json const& value) { return value.iterate_object(); });
      if (!configs) return std::unexpected(std::move(configs.error()));
      if (group.collection.type == CollectionType::Generic) {
        auto& entries = std::get<0>(group.entries);
        for (auto const& [id, value] : *configs) {
          auto entry = parse_generic(id, value);
          if (!entry) return std::unexpected(std::move(entry.error()));
          entries.try_emplace(id, std::move(*entry));
        }
      } else {
        auto entries = std::make_unique<LazyEntries>();
        entries->source = file;
        for (auto const& [id, value] : *configs)
          entries->slots.try_emplace(id, Slot{value, {}});
        group.entries = std::move(entries);
      }
      result->m_groups[group.collection.name] = std::move(group);
      return {};
    };
    auto loaded = read_file();
    if (!loaded)
      return std::unexpected((*assets)->with_asset_context(std::move(loaded.error()), file, "Loading collection"));
  }
  return result;
}

List<Collection> CollectionDatabase::collections() const {
  List<Collection> result;
  result.reserve(m_groups.size());
  for (auto const& entry : m_groups) result.push_back(entry.second.collection);
  return result;
}

std::expected<Collection, StarException> CollectionDatabase::collection(String const& name) const {
  if (auto group = m_groups.find_ref(name)) return group->get().collection;
  return std::unexpected(CollectionDatabaseException::format("Collection '{}' not found", name));
}

std::expected<List<Collectable>, StarException> CollectionDatabase::collectables(String const& name, ItemCatalog catalog) const {
  auto found = m_groups.find_ref(name);
  if (!found) return std::unexpected(CollectionDatabaseException::format("Collection '{}' not found", name));
  auto const& group = found->get();
  if (group.entries.index() == 0) return std::get<0>(group.entries).values();
  auto const& entries = *std::get<1>(group.entries);
  bool complete;
  {
    std::lock_guard lock(entries.mutex);
    complete = entries.ready == entries.slots.size();
  }
  // Published slots never change. Once complete the whole group can be copied without the lock.
  List<Collectable> result;
  result.reserve(entries.slots.size());
  for (auto const& [id, slot] : entries.slots) {
    if (complete) {
      result.push_back(*slot.published);
    } else {
      auto value = resolve(group.collection, entries, id, slot, catalog);
      if (!value) return std::unexpected(std::move(value.error()));
      result.push_back(std::move(*value));
    }
  }
  return result;
}

std::expected<Collectable, StarException> CollectionDatabase::collectable(String const& name, String const& id, ItemCatalog catalog) const {
  if (auto found = m_groups.find_ref(name)) {
    auto const& group = found->get();
    if (group.entries.index() == 0) {
      if (auto value = std::get<0>(group.entries).find_ref(id)) return value->get();
    } else {
      auto const& entries = *std::get<1>(group.entries);
      if (auto slot = entries.slots.find_ref(id)) return resolve(group.collection, entries, id, slot->get(), catalog);
    }
  }
  return std::unexpected(CollectionDatabaseException::format("Collectable '{}' not found in collection '{}'", id, name));
}

std::expected<Collectable, StarException> CollectionDatabase::resolve(Collection const& collection, LazyEntries const& entries, String const& id, Slot const& slot, ItemCatalog catalog) const {
  {
    std::lock_guard lock(entries.mutex);
    if (slot.published) return *slot.published;
  }
  auto value = collection.type == CollectionType::Item ? parse_item(id, slot.config, catalog) : parse_monster(id, slot.config);
  if (!value) {
    auto error = std::move(value.error());
    if (error.is<MapException>())
      error = CollectionDatabaseException(Star::format("Collectable '{}' not found in collection '{}'", id, collection.name), std::move(error));
    return std::unexpected(CollectionDatabaseException(Star::format("Resolving collectable '{}' from '{}'", id, entries.source), std::move(error)));
  }
  std::lock_guard lock(entries.mutex);
  if (!slot.published) {
    slot.published.emplace(std::move(*value));
    ++entries.ready;
  }
  return *slot.published;
}

bool CollectionDatabase::contains(String const& name, String const& id) const {
  auto group = m_groups.find_ref(name);
  if (!group) return false;
  auto const& entries = group->get().entries;
  return entries.index() == 0 ? std::get<0>(entries).contains(id) : std::get<1>(entries)->slots.contains(id);
}

namespace {
std::expected<Collectable, StarException> parse_generic(String const& name, Json const& config) {
  auto order = config.get_int("order", 0);
  if (!order)
    return std::unexpected(std::move(order.error()));
  auto title = config.get_string("title", "");
  if (!title)
    return std::unexpected(std::move(title.error()));
  auto description = config.get_string("description", "");
  if (!description)
    return std::unexpected(std::move(description.error()));
  auto icon = config.get_string("icon", "");
  if (!icon)
    return std::unexpected(std::move(icon.error()));
  return Collectable{name, static_cast<int>(*order), std::move(*title), std::move(*description), std::move(*icon)};
}

} // namespace

std::expected<Collectable, StarException> CollectionDatabase::parse_monster(String const& name, Json const& config) const {
  auto collectable = parse_generic(name, config);
  if (!collectable)
    return std::unexpected(std::move(collectable.error()));
  auto monster_type = config.get_string("monsterType");
  if (!monster_type)
    return std::unexpected(std::move(monster_type.error()));
  auto database = m_root.monsterDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto variant = (*database)->monsterVariant(*monster_type, 0);
  if (!variant)
    return std::unexpected(std::move(variant.error()));
  collectable->title = std::move(variant->shortDescription).value_or("");
  collectable->description = std::move(variant->description).value_or("");
  return std::move(*collectable);
}

std::expected<Collectable, StarException> CollectionDatabase::parse_item(String const& name, Json const& config, ItemCatalog catalog) const {
  auto collectable = parse_generic(name, config);
  if (!collectable)
    return std::unexpected(std::move(collectable.error()));
  std::shared_ptr<ItemDatabase const> catalog_owner;
  if (!catalog) {
    auto loaded = m_root.itemDatabase();
    if (!loaded)
      return std::unexpected(std::move(loaded.error()));
    catalog_owner = std::move(*loaded);
    catalog = std::cref(*catalog_owner);
  }
  auto item_name = config.get_string("item");
  if (!item_name)
    return std::unexpected(std::move(item_name.error()));
  auto descriptor = ItemDescriptor::create(*item_name, 1);
  if (!descriptor)
    return std::unexpected(std::move(descriptor.error()));
  auto item = catalog->get().itemShared(std::move(*descriptor));
  if (!item)
    return std::unexpected(std::move(item.error()));
  if (!*item)
    return std::unexpected(CollectionDatabaseException(Star::format("Collectable '{}' refers to an empty item", name)));
  collectable->title = (*item)->friendly_name();
  collectable->description = (*item)->description();
  auto configured_icon = config.find("icon");
  if (!configured_icon) return std::unexpected(std::move(configured_icon.error()));
  if (!*configured_icon || (*configured_icon)->get().is_null()) {
    auto inventory_icon = (*item)->instance_value("inventoryIcon", "");
    if (!inventory_icon)
      return std::unexpected(std::move(inventory_icon.error()));
    if (inventory_icon->is_type(Json::Type::String)) {
      // The resolved item owns its actual directory, including rebuilder replacements.
      auto icon = inventory_icon->to_string();
      if (!icon)
        return std::unexpected(std::move(icon.error()));
      collectable->icon = AssetPath::relative_to((*item)->directory(), *icon);
    }
  }
  return std::move(*collectable);
}

}
