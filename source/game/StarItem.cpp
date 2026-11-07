import star.json;
import star.json_extra;
import star.set;
import star.string;
import star.data_stream;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.bi_map;
import star.game_types;
import star.strong_typedef;
import star.vector;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.listener;
import std;
import star.version;
import star.random;
import star.logging;

import star.item;
import star.memory;
import star.format;
import star.drawable;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

namespace Star {

Item::Item(Root& root) : m_root(root), m_count(1), m_maxStack(0), m_rarity(), m_twoHanded(false), m_timeToLive(0), m_price(0) {}

std::expected<void, StarException> Item::initialize(Json config, String directory, Json parameters) {
  m_config = std::move(config);
  m_directory = std::move(directory);
  m_parameters = std::move(parameters);
  if (!m_parameters.isType(Json::Type::Object))
    return std::unexpected(ItemException("Item parameters not an object"));
  auto name = m_config.getString("itemName");
  if (!name)
    return std::unexpected(std::move(name.error()));
  m_name = std::move(*name);

  auto assets = root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto defaults = (*assets)->json("/items/defaultParameters.config");
  if (!defaults)
    return std::unexpected(std::move(defaults.error()));
  auto defaultMaxStack = defaults->get("defaultMaxStack").and_then([](Json const& value) { return value.toInt(); });
  if (!defaultMaxStack)
    return std::unexpected(std::move(defaultMaxStack.error()));
  auto maxStack = instanceValue("maxStack", *defaultMaxStack).and_then([](Json const& value) { return value.toInt(); });
  if (!maxStack)
    return std::unexpected(std::move(maxStack.error()));
  m_maxStack = *maxStack;
  auto shortDescription = instanceValue("shortdescription", "").and_then([](Json const& value) { return value.toString(); });
  if (!shortDescription)
    return std::unexpected(std::move(shortDescription.error()));
  m_shortDescription = std::move(*shortDescription);
  auto description = instanceValue("description", "").and_then([](Json const& value) { return value.toString(); });
  if (!description)
    return std::unexpected(std::move(description.error()));
  m_description = std::move(*description);
  auto rarity = instanceValue("rarity").and_then([](Json const& value) { return value.toString(); });
  if (!rarity)
    return std::unexpected(std::move(rarity.error()));
  auto rarityValue = enumValue<RarityNames>(*rarity);
  if (!rarityValue)
    return std::unexpected(std::move(rarityValue.error()));
  m_rarity = *rarityValue;

  auto metadata = root().imageMetadataDatabase();
  if (!metadata)
    return std::unexpected(std::move(metadata.error()));
  auto loadIcon = [&](Json const& icon) -> std::expected<List<Drawable>, StarException> {
    List<Drawable> drawables;
    if (icon.type() == Json::Type::Array) {
      auto array = icon.iterateArray();
      if (!array)
        return std::unexpected(std::move(array.error()));
      for (auto imageConfig : *array) {
        auto image = imageConfig.optString("image");
        if (!image)
          return std::unexpected(std::move(image.error()));
        if (*image) {
          auto relative = imageConfig.set("image", AssetPath::relative_to(m_directory, **image));
          if (!relative)
            return std::unexpected(std::move(relative.error()));
          imageConfig = std::move(*relative);
        }
        auto drawable = Drawable::create(**metadata, imageConfig);
        if (!drawable)
          return std::unexpected(std::move(drawable.error()));
        drawables.append(std::move(*drawable));
      }
    } else {
      auto image = icon.toString();
      if (!image)
        return std::unexpected(std::move(image.error()));
      auto drawable = Drawable::makeImage(**metadata, AssetPath::relative_to(m_directory, *image), 1.0f, true, Vec2F());
      if (!drawable)
        return std::unexpected(std::move(drawable.error()));
      drawables.append(std::move(*drawable));
    }
    return drawables;
  };
  auto missingIcon = defaults->get("missingIcon");
  if (!missingIcon)
    return std::unexpected(std::move(missingIcon.error()));
  auto inventoryIcon = instanceValue("inventoryIcon", *missingIcon);
  if (!inventoryIcon)
    return std::unexpected(std::move(inventoryIcon.error()));
  auto icons = loadIcon(*inventoryIcon);
  if (!icons)
    return std::unexpected(std::move(icons.error()));
  auto setIcons = setIconDrawables(std::move(*icons));
  if (!setIcons)
    return std::unexpected(std::move(setIcons.error()));
  auto secondaryIcon = instanceValue("secondaryIcon", Json());
  if (!secondaryIcon)
    return std::unexpected(std::move(secondaryIcon.error()));
  if (secondaryIcon->isType(Json::Type::Array) || secondaryIcon->isType(Json::Type::String)) {
    auto secondary = loadIcon(*secondaryIcon);
    if (!secondary)
      return std::unexpected(std::move(secondary.error()));
    auto setSecondary = setSecondaryIconDrawables(std::move(*secondary));
    if (!setSecondary)
      return std::unexpected(std::move(setSecondary.error()));
  } else {
    m_secondaryIconDrawables.reset();
  }

  auto twoHanded = instanceValue("twoHanded", false).and_then([](Json const& value) { return value.toBool(); });
  if (!twoHanded)
    return std::unexpected(std::move(twoHanded.error()));
  m_twoHanded = *twoHanded;
  auto defaultPrice = defaults->get("defaultPrice");
  if (!defaultPrice)
    return std::unexpected(std::move(defaultPrice.error()));
  auto price = instanceValue("price", *defaultPrice).and_then([](Json const& value) { return value.toInt(); });
  if (!price)
    return std::unexpected(std::move(price.error()));
  m_price = *price;
  auto tooltipKind = instanceValue("tooltipKind", "").and_then([](Json const& value) { return value.toString(); });
  if (!tooltipKind)
    return std::unexpected(std::move(tooltipKind.error()));
  m_tooltipKind = std::move(*tooltipKind);
  auto largeImage = instanceValue("largeImage");
  if (!largeImage)
    return std::unexpected(std::move(largeImage.error()));
  if (!largeImage->isNull()) {
    auto image = largeImage->toString();
    if (!image)
      return std::unexpected(std::move(image.error()));
    m_largeImage = AssetPath::relative_to(m_directory, *image);
  }
  auto category = instanceValue("category", "").and_then([](Json const& value) { return value.toString(); });
  if (!category)
    return std::unexpected(std::move(category.error()));
  m_category = std::move(*category);
  auto pickupSounds = m_config.get("pickupSounds", JsonArray{}).and_then([](Json const& value) { return jsonToStringSet(value); });
  if (!pickupSounds)
    return std::unexpected(std::move(pickupSounds.error()));
  m_pickupSounds = std::move(*pickupSounds);
  if (m_pickupSounds.empty()) {
    auto sounds = defaults->get("pickupSounds").and_then([](Json const& value) { return jsonToStringSet(value); });
    if (!sounds)
      return std::unexpected(std::move(sounds.error()));
    m_pickupSounds = std::move(*sounds);
  }
  auto defaultTimeToLive = defaults->get("defaultTimeToLive").and_then([](Json const& value) { return value.toFloat(); });
  if (!defaultTimeToLive)
    return std::unexpected(std::move(defaultTimeToLive.error()));
  auto timeToLive = instanceValue("timeToLive", *defaultTimeToLive).and_then([](Json const& value) { return value.toFloat(); });
  if (!timeToLive)
    return std::unexpected(std::move(timeToLive.error()));
  m_timeToLive = *timeToLive;
  auto blueprints = instanceValue("learnBlueprintsOnPickup", JsonArray{}).and_then([](Json const& value) { return jsonToStringList(value); });
  if (!blueprints)
    return std::unexpected(std::move(blueprints.error()));
  for (auto const& blueprint : *blueprints) {
    auto descriptor = ItemDescriptor::create(Json(blueprint));
    if (!descriptor)
      return std::unexpected(std::move(descriptor.error()));
    m_learnBlueprintsOnPickup.append(std::move(*descriptor));
  }
  auto collectables = instanceValue("collectablesOnPickup", JsonObject{}).and_then([](Json const& value) { return value.iterateObject(); });
  if (!collectables)
    return std::unexpected(std::move(collectables.error()));
  for (auto const& pair : *collectables) {
    auto value = pair.second.toString();
    if (!value)
      return std::unexpected(std::move(value.error()));
    m_collectablesOnPickup[pair.first] = std::move(*value);
  }
  return {};
}

Item::~Item() = default;

Root& Item::root() const {
  return m_root.get();
}

String Item::name() const {
  return m_name;
}

std::uint64_t Item::count() const {
  return m_count;
}

std::uint64_t Item::setCount(std::uint64_t count, bool overfill) {
  if (overfill)
    m_count = count;
  else
    m_count = std::min(count, m_maxStack);
  return count - m_count;
}

bool Item::stackableWith(std::optional<std::reference_wrapper<Item const>> item) const {
  return item && name() == item->get().name() && parameters() == item->get().parameters();
}

std::uint64_t Item::maxStack() const {
  return m_maxStack;
}

std::uint64_t Item::couldStack(std::optional<std::reference_wrapper<Item const>> item) const {
  if (stackableWith(item) && m_count < m_maxStack) {
    std::uint64_t take = m_maxStack - m_count;
    return std::min(take, item->get().count());
  } else {
    return 0;
  }
}

bool Item::stackWith(std::optional<std::reference_wrapper<Item>> item) {
  std::uint64_t take = couldStack(item);

  if (take > 0 && item->get().consume(take)) {
    m_count += take;
    return true;
  } else {
    return false;
  }
}

bool Item::matches(ItemDescriptor const& descriptor, bool exactMatch) const {
  return descriptor.name() == m_name && (!exactMatch || descriptor.parameters() == m_parameters);
}

bool Item::matches(Item const& other, bool exactMatch) const {
  return other.name() == m_name && (!exactMatch || other.parameters() == m_parameters);
}

bool Item::consume(std::uint64_t count) {
  if (m_count >= count) {
    m_count -= count;
    return true;
  } else {
    return false;
  }
}

std::expected<std::shared_ptr<Item>, StarException> Item::take(std::uint64_t max) {
  std::uint64_t takeCount = std::min(m_count, max);
  if (takeCount == 0)
    return std::shared_ptr<Item>();
  auto newItems = clone();
  if (!newItems)
    return std::unexpected(std::move(newItems.error()));
  if (*newItems) {
    m_count -= takeCount;
    (*newItems)->setCount(takeCount, false);
    return std::move(*newItems);
  }
  auto logged = root().logger().warn("Could not clone {}, not moving {} items as requested.", friendlyName(), takeCount);
  if (!logged)
    return std::unexpected(std::move(logged.error()));
  return std::shared_ptr<Item>();
}

bool Item::empty() const {
  return m_count == 0;
}

ItemDescriptor Item::descriptor() const {
  return ItemDescriptor(m_name, m_count, m_parameters, {});
}

String Item::description() const {
  return m_description;
}

String Item::friendlyName() const {
  return m_shortDescription;
}

Rarity Item::rarity() const {
  return m_rarity;
}

std::expected<List<Drawable>, StarException> Item::iconDrawables() const {
  return m_iconDrawables;
}

std::expected<std::optional<List<Drawable>>, StarException> Item::secondaryDrawables() const {
  return m_secondaryIconDrawables;
}

bool Item::hasSecondaryDrawables() const {
  return m_secondaryIconDrawables.has_value();
}

std::expected<List<Drawable>, StarException> Item::dropDrawables(std::optional<std::reference_wrapper<TileDrawer>>) const {
  auto drawables = iconDrawables();
  if (!drawables)
    return std::unexpected(std::move(drawables.error()));
  Drawable::scaleAll(*drawables, 1.0f / TilePixels);
  return drawables;
}

bool Item::twoHanded() const {
  return m_twoHanded;
}

float Item::timeToLive() const {
  return m_timeToLive;
}

std::uint64_t Item::price() const {
  return m_price * count();
}

String Item::tooltipKind() const {
  return m_tooltipKind;
}

String Item::largeImage() const {
  return m_largeImage;
}

String Item::category() const {
  return m_category;
}

std::expected<String, StarException> Item::pickupSound() const {
  auto sound = root().random().randFrom(m_pickupSounds);
  if (!sound)
    return std::unexpected(std::move(sound.error()));
  return sound->get();
}

void Item::setMaxStack(std::uint64_t maxStack) {
  m_maxStack = maxStack;
}

void Item::setDescription(String const& description) {
  m_description = description;
}

void Item::setShortDescription(String const& description) {
  m_shortDescription = description;
}

void Item::setRarity(Rarity rarity) {
  m_rarity = rarity;
}

void Item::setPrice(std::uint64_t price) {
  m_price = price;
}

std::expected<void, StarException> Item::setIconDrawables(List<Drawable> drawables) {
  auto metadata = root().imageMetadataDatabase();
  if (!metadata)
    return std::unexpected(std::move(metadata.error()));
  auto boundBox = Drawable::boundBoxAll(**metadata, drawables, true);
  if (!boundBox)
    return std::unexpected(std::move(boundBox.error()));
  if (!boundBox->isEmpty()) {
    for (auto& drawable : drawables)
      drawable.translate(-boundBox->center());
    float zoom = 16.0f / std::max(boundBox->width(), boundBox->height());
    if (zoom < 1) {
      for (auto& drawable : drawables)
        drawable.scale(zoom);
    }
  }
  m_iconDrawables = std::move(drawables);
  return {};
}

std::expected<void, StarException> Item::setSecondaryIconDrawables(std::optional<List<Drawable>> drawables) {
  if (!drawables) {
    m_secondaryIconDrawables.reset();
    return {};
  }
  auto metadata = root().imageMetadataDatabase();
  if (!metadata)
    return std::unexpected(std::move(metadata.error()));
  auto boundBox = Drawable::boundBoxAll(**metadata, *drawables, true);
  if (!boundBox)
    return std::unexpected(std::move(boundBox.error()));
  if (!boundBox->isEmpty()) {
    for (auto& drawable : *drawables)
      drawable.translate(-boundBox->center());
    float zoom = 16.0f / std::max(boundBox->width(), boundBox->height());
    if (zoom < 1) {
      for (auto& drawable : *drawables)
        drawable.scale(zoom);
    }
  }
  m_secondaryIconDrawables = std::move(drawables);
  return {};
}

void Item::setTwoHanded(bool twoHanded) {
  m_twoHanded = twoHanded;
}

void Item::setTimeToLive(float timeToLive) {
  m_timeToLive = timeToLive;
}

std::expected<List<QuestArcDescriptor>, StarException> Item::pickupQuestTemplates() const {
  auto templates = instanceValue("pickupQuestTemplates", JsonArray{}).and_then([](Json const& value) { return value.iterateArray(); });
  if (!templates)
    return std::unexpected(std::move(templates.error()));
  List<QuestArcDescriptor> result;
  for (auto const& quest : *templates) {
    auto descriptor = QuestArcDescriptor::fromJson(root(), quest);
    if (!descriptor)
      return std::unexpected(std::move(descriptor.error()));
    result.append(std::move(*descriptor));
  }
  return result;
}

std::expected<StringSet, StarException> Item::itemTags() const {
  return m_config.get("itemTags", JsonArray{}).and_then([](Json const& value) { return jsonToStringSet(value); });
}

std::expected<bool, StarException> Item::hasItemTag(String const& itemTag) const {
  auto tags = itemTags();
  if (!tags)
    return std::unexpected(std::move(tags.error()));
  return tags->contains(itemTag);
}

std::expected<Json, StarException> Item::instanceValue(String const& name, Json const& def) const {
  return jsonMergeQueryDef(name, def, m_config, m_parameters);
}

std::expected<Json, StarException> Item::instanceValueOfType(String const& name, Json::Type type, Json const& def) const {
  auto value = instanceValue(name, def);
  if (!value)
    return std::unexpected(std::move(value.error()));
  if (value->isType(type))
    return value;
  return def;
}

std::expected<Json, StarException> Item::instanceValues() const {
  auto parameters = m_parameters.toObject();
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  return m_config.setAll(std::move(*parameters));
}

Json Item::config() const {
  return m_config;
}

Json Item::parameters() const {
  return m_parameters;
}

std::expected<void, StarException> Item::setInstanceValue(String const& name, Json const& value) {
  auto current = m_parameters.get(name, {});
  if (!current)
    return std::unexpected(std::move(current.error()));
  if (*current != value) {
    auto parameters = m_parameters.setAll(JsonObject{{name, value}});
    if (!parameters)
      return std::unexpected(std::move(parameters.error()));
    m_parameters = std::move(*parameters);
  }
  return {};
}

String const& Item::directory() const {
  return m_directory;
}

List<ItemDescriptor> Item::learnBlueprintsOnPickup() const {
  return m_learnBlueprintsOnPickup;
}

StringMap<String> Item::collectablesOnPickup() const {
  return m_collectablesOnPickup;
}

std::expected<std::shared_ptr<GenericItem>, StarException> GenericItem::create(Root& root, Json const& config, String const& directory, Json const& parameters) {
  auto item = std::shared_ptr<GenericItem>(new GenericItem(root));
  auto initialized = item->initialize(config, directory, parameters);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return item;
}

std::expected<std::shared_ptr<Item>, StarException> GenericItem::clone() const {
  return std::make_shared<GenericItem>(*this);
}

bool Item::itemsEqual(std::optional<std::reference_wrapper<Item const>> a, std::optional<std::reference_wrapper<Item const>> b) {
  if (!a && !b) // Both are null
    return true;
  if (a && b) // Both aren't null, compare
    return a->get().stackableWith(b);
  else // One is null, so not equal
    return false;
}

}
