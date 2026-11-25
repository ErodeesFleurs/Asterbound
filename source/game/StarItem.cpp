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
import star.image_metadata_database;

namespace Star {

namespace {
std::expected<void, StarException> normalize_icon_drawables(ImageMetadataDatabase const& metadata, List<Drawable>& drawables) {
  auto bounds = Drawable::bound_box_all(metadata, drawables, true);
  if (!bounds)
    return std::unexpected(std::move(bounds.error()));
  if (!bounds->is_empty()) {
    auto offset = -bounds->center();
    float zoom = 16.0f / std::max(bounds->width(), bounds->height());
    for (auto& drawable : drawables) {
      drawable.translate(offset);
      if (zoom < 1)
        drawable.scale(zoom);
    }
  }
  return {};
}
}

Item::Item(Root& root) : m_root(root), m_count(1), m_max_stack(0), m_rarity(), m_two_handed(false), m_time_to_live(0), m_price(0) {}

std::expected<void, StarException> Item::initialize(Json config, String directory, Json parameters) {
  m_config = std::move(config);
  m_directory = std::move(directory);
  m_parameters = std::move(parameters);
  if (!m_parameters.is_type(Json::Type::Object))
    return std::unexpected(ItemException("Item parameters not an object"));
  auto name = m_config.get_string("itemName");
  if (!name)
    return std::unexpected(std::move(name.error()));
  m_name = std::move(*name);

  auto assets = root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto defaults = (*assets)->json("/items/defaultParameters.config");
  if (!defaults)
    return std::unexpected(std::move(defaults.error()));
  auto default_max_stack = defaults->get("defaultMaxStack").and_then([](Json const& value) { return value.to_int(); });
  if (!default_max_stack)
    return std::unexpected(std::move(default_max_stack.error()));
  auto max_stack = instance_value("maxStack", *default_max_stack).and_then([](Json const& value) { return value.to_int(); });
  if (!max_stack)
    return std::unexpected(std::move(max_stack.error()));
  m_max_stack = *max_stack;
  auto short_description = instance_value("shortdescription", "").and_then([](Json const& value) { return value.to_string(); });
  if (!short_description)
    return std::unexpected(std::move(short_description.error()));
  m_short_description = std::move(*short_description);
  auto description = instance_value("description", "").and_then([](Json const& value) { return value.to_string(); });
  if (!description)
    return std::unexpected(std::move(description.error()));
  m_description = std::move(*description);
  auto rarity = instance_value("rarity").and_then([](Json const& value) { return value.to_string(); });
  if (!rarity)
    return std::unexpected(std::move(rarity.error()));
  auto rarity_value = enum_value<rarity_names>(*rarity);
  if (!rarity_value)
    return std::unexpected(std::move(rarity_value.error()));
  m_rarity = *rarity_value;

  auto metadata = root().image_metadata_database();
  if (!metadata)
    return std::unexpected(std::move(metadata.error()));
  auto load_icon = [&](Json const& icon) -> std::expected<List<Drawable>, StarException> {
    List<Drawable> drawables;
    if (icon.type() == Json::Type::Array) {
      auto array = icon.iterate_array();
      if (!array)
        return std::unexpected(std::move(array.error()));
      for (auto image_config : *array) {
        auto image = image_config.opt_string("image");
        if (!image)
          return std::unexpected(std::move(image.error()));
        if (*image) {
          auto relative = image_config.set("image", AssetPath::relative_to(m_directory, **image));
          if (!relative)
            return std::unexpected(std::move(relative.error()));
          image_config = std::move(*relative);
        }
        auto drawable = Drawable::create(**metadata, image_config);
        if (!drawable)
          return std::unexpected(std::move(drawable.error()));
        drawables.push_back(std::move(*drawable));
      }
    } else {
      auto image = icon.to_string();
      if (!image)
        return std::unexpected(std::move(image.error()));
      auto drawable = Drawable::make_image(**metadata, AssetPath::relative_to(m_directory, *image), 1.0f, true, Vec2F());
      if (!drawable)
        return std::unexpected(std::move(drawable.error()));
      drawables.push_back(std::move(*drawable));
    }
    return drawables;
  };
  auto missing_icon = defaults->get("missingIcon");
  if (!missing_icon)
    return std::unexpected(std::move(missing_icon.error()));
  auto inventory_icon = instance_value("inventoryIcon", *missing_icon);
  if (!inventory_icon)
    return std::unexpected(std::move(inventory_icon.error()));
  auto icons = load_icon(*inventory_icon);
  if (!icons)
    return std::unexpected(std::move(icons.error()));
  if (auto result = normalize_icon_drawables(**metadata, *icons); !result)
    return result;
  m_icon_drawables = std::move(*icons);
  auto secondary_icon = instance_value("secondaryIcon", Json());
  if (!secondary_icon)
    return std::unexpected(std::move(secondary_icon.error()));
  if (secondary_icon->is_type(Json::Type::Array) || secondary_icon->is_type(Json::Type::String)) {
    auto secondary = load_icon(*secondary_icon);
    if (!secondary)
      return std::unexpected(std::move(secondary.error()));
    if (auto result = normalize_icon_drawables(**metadata, *secondary); !result)
      return result;
    m_secondary_icon_drawables = std::move(*secondary);
  } else {
    m_secondary_icon_drawables.reset();
  }

  auto two_handed = instance_value("twoHanded", false).and_then([](Json const& value) { return value.to_bool(); });
  if (!two_handed)
    return std::unexpected(std::move(two_handed.error()));
  m_two_handed = *two_handed;
  auto default_price = defaults->get("defaultPrice");
  if (!default_price)
    return std::unexpected(std::move(default_price.error()));
  auto price = instance_value("price", *default_price).and_then([](Json const& value) { return value.to_int(); });
  if (!price)
    return std::unexpected(std::move(price.error()));
  m_price = *price;
  auto tooltip_kind = instance_value("tooltipKind", "").and_then([](Json const& value) { return value.to_string(); });
  if (!tooltip_kind)
    return std::unexpected(std::move(tooltip_kind.error()));
  m_tooltip_kind = std::move(*tooltip_kind);
  auto large_image = instance_value("largeImage");
  if (!large_image)
    return std::unexpected(std::move(large_image.error()));
  if (!large_image->is_null()) {
    auto image = large_image->to_string();
    if (!image)
      return std::unexpected(std::move(image.error()));
    m_large_image = AssetPath::relative_to(m_directory, *image);
  }
  auto category = instance_value("category", "").and_then([](Json const& value) { return value.to_string(); });
  if (!category)
    return std::unexpected(std::move(category.error()));
  m_category = std::move(*category);
  auto pickup_sounds = m_config.get("pickupSounds", JsonArray{}).and_then([](Json const& value) { return json_to_string_set(value); });
  if (!pickup_sounds)
    return std::unexpected(std::move(pickup_sounds.error()));
  m_pickup_sounds = std::move(*pickup_sounds);
  if (m_pickup_sounds.empty()) {
    auto sounds = defaults->get("pickupSounds").and_then([](Json const& value) { return json_to_string_set(value); });
    if (!sounds)
      return std::unexpected(std::move(sounds.error()));
    m_pickup_sounds = std::move(*sounds);
  }
  auto default_time_to_live = defaults->get("defaultTimeToLive").and_then([](Json const& value) { return value.to_float(); });
  if (!default_time_to_live)
    return std::unexpected(std::move(default_time_to_live.error()));
  auto time_to_live = instance_value("timeToLive", *default_time_to_live).and_then([](Json const& value) { return value.to_float(); });
  if (!time_to_live)
    return std::unexpected(std::move(time_to_live.error()));
  m_time_to_live = *time_to_live;
  auto blueprints = instance_value("learnBlueprintsOnPickup", JsonArray{}).and_then([](Json const& value) { return json_to_string_list(value); });
  if (!blueprints)
    return std::unexpected(std::move(blueprints.error()));
  for (auto const& blueprint : *blueprints) {
    auto descriptor = ItemDescriptor::create(Json(blueprint));
    if (!descriptor)
      return std::unexpected(std::move(descriptor.error()));
    m_learn_blueprints_on_pickup.push_back(std::move(*descriptor));
  }
  auto collectables = instance_value("collectablesOnPickup", JsonObject{}).and_then([](Json const& value) {
    return value.iterate_object();
  });
  if (!collectables)
    return std::unexpected(std::move(collectables.error()));
  for (auto const& pair : *collectables) {
    auto value = pair.second.to_string();
    if (!value)
      return std::unexpected(std::move(value.error()));
    m_collectables_on_pickup[pair.first] = std::move(*value);
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

std::uint64_t Item::set_count(std::uint64_t count, bool overfill) {
  if (overfill)
    m_count = count;
  else
    m_count = std::min(count, m_max_stack);
  return count - m_count;
}

bool Item::stackable_with(std::optional<std::reference_wrapper<Item const>> item) const {
  return item && matches(item->get(), true);
}

std::uint64_t Item::max_stack() const {
  return m_max_stack;
}

std::uint64_t Item::could_stack(std::optional<std::reference_wrapper<Item const>> item) const {
  if (m_count >= m_max_stack || !item || !item->get().count() || !stackable_with(item))
    return 0;
  return std::min(m_max_stack - m_count, item->get().count());
}

bool Item::stack_with(std::optional<std::reference_wrapper<Item>> item) {
  std::uint64_t taken = could_stack(item);

  if (taken > 0 && item->get().consume(taken)) {
    m_count += taken;
    return true;
  } else {
    return false;
  }
}

bool Item::matches(ItemDescriptor const& descriptor, bool exact_match) const {
  return descriptor.name() == m_name && (!exact_match || descriptor.parameters() == m_parameters);
}

bool Item::matches(Item const& other, bool exact_match) const {
  return other.m_name == m_name && (!exact_match || other.m_parameters == m_parameters);
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
  std::uint64_t take_count = std::min(m_count, max);
  if (take_count == 0)
    return std::shared_ptr<Item>();
  auto new_items = clone();
  if (!new_items)
    return std::unexpected(std::move(new_items.error()));
  if (*new_items) {
    auto overflow = (*new_items)->set_count(take_count, false);
    if (overflow == take_count)
      return std::shared_ptr<Item>();
    m_count -= take_count - overflow;
    return std::move(*new_items);
  }
  auto logged = root().logger().warn("Could not clone {}, not moving {} items as requested.", friendly_name(), take_count);
  if (!logged)
    return std::unexpected(std::move(logged.error()));
  return std::shared_ptr<Item>();
}

bool Item::empty() const {
  return m_count == 0;
}

ItemDescriptor Item::descriptor() const {
  return ItemDescriptor(m_name, m_count, m_parameters);
}

String Item::description() const {
  return m_description;
}

String Item::friendly_name() const {
  return m_short_description;
}

Rarity Item::rarity() const {
  return m_rarity;
}

std::expected<List<Drawable>, StarException> Item::icon_drawables() const {
  return m_icon_drawables;
}

std::expected<std::optional<List<Drawable>>, StarException> Item::secondary_drawables() const {
  return m_secondary_icon_drawables;
}

bool Item::has_secondary_drawables() const {
  return m_secondary_icon_drawables.has_value();
}

std::expected<List<Drawable>, StarException> Item::drop_drawables(std::optional<std::reference_wrapper<TileDrawer>>) const {
  auto drawables = icon_drawables();
  if (!drawables)
    return std::unexpected(std::move(drawables.error()));
  Drawable::scale_all(*drawables, 1.0f / tile_pixels);
  return drawables;
}

bool Item::two_handed() const {
  return m_two_handed;
}

float Item::time_to_live() const {
  return m_time_to_live;
}

std::uint64_t Item::price() const {
  return m_price * count();
}

String Item::tooltip_kind() const {
  return m_tooltip_kind;
}

String Item::large_image() const {
  return m_large_image;
}

String Item::category() const {
  return m_category;
}

std::expected<String, StarException> Item::pickup_sound() const {
  auto sound = root().random().sample_ref(m_pickup_sounds);
  if (!sound)
    return std::unexpected(std::move(sound.error()));
  return sound->get();
}

void Item::set_max_stack(std::uint64_t max_stack) {
  m_max_stack = max_stack;
}

void Item::set_description(String const& description) {
  m_description = description;
}

void Item::set_short_description(String const& description) {
  m_short_description = description;
}

void Item::set_rarity(Rarity rarity) {
  m_rarity = rarity;
}

void Item::set_price(std::uint64_t price) {
  m_price = price;
}

std::expected<void, StarException> Item::set_icon_drawables(List<Drawable> drawables) {
  auto metadata = root().image_metadata_database();
  if (!metadata)
    return std::unexpected(std::move(metadata.error()));
  if (auto result = normalize_icon_drawables(**metadata, drawables); !result)
    return result;
  m_icon_drawables = std::move(drawables);
  return {};
}

std::expected<void, StarException> Item::set_secondary_icon_drawables(std::optional<List<Drawable>> drawables) {
  if (!drawables) {
    m_secondary_icon_drawables.reset();
    return {};
  }
  auto metadata = root().image_metadata_database();
  if (!metadata)
    return std::unexpected(std::move(metadata.error()));
  if (auto result = normalize_icon_drawables(**metadata, *drawables); !result)
    return result;
  m_secondary_icon_drawables = std::move(drawables);
  return {};
}

void Item::set_two_handed(bool two_handed) {
  m_two_handed = two_handed;
}

void Item::set_time_to_live(float time_to_live) {
  m_time_to_live = time_to_live;
}

std::expected<List<QuestArcDescriptor>, StarException> Item::pickup_quest_templates() const {
  auto templates = instance_value("pickupQuestTemplates", JsonArray{}).and_then([](Json const& value) {
    return value.iterate_array();
  });
  if (!templates)
    return std::unexpected(std::move(templates.error()));
  List<QuestArcDescriptor> result;
  for (auto const& quest : *templates) {
    auto descriptor = QuestArcDescriptor::fromJson(root(), quest);
    if (!descriptor)
      return std::unexpected(std::move(descriptor.error()));
    result.push_back(std::move(*descriptor));
  }
  return result;
}

std::expected<StringSet, StarException> Item::item_tags() const {
  return m_config.get("itemTags", JsonArray{}).and_then([](Json const& value) { return json_to_string_set(value); });
}

std::expected<bool, StarException> Item::has_item_tag(String const& item_tag) const {
  auto config_tags = m_config.find("itemTags");
  if (!config_tags)
    return std::unexpected(std::move(config_tags.error()));
  if (!*config_tags)
    return false;
  auto tags = (**config_tags).get().iterate_array();
  if (!tags)
    return std::unexpected(std::move(tags.error()));
  bool found = false;
  for (auto const& entry : *tags) {
    auto tag = entry.string_ptr();
    if (!tag)
      return std::unexpected(std::move(tag.error()));
    found |= **tag == item_tag;
  }
  return found;
}

std::expected<Json, StarException> Item::instance_value(String const& name, Json const& def) const {
  return json_merge_query_def(name, def, m_config, m_parameters);
}

std::expected<Json, StarException> Item::instance_value_of_type(String const& name, Json::Type type, Json const& def) const {
  auto value = instance_value(name, def);
  if (!value)
    return std::unexpected(std::move(value.error()));
  if (value->is_type(type))
    return value;
  return def;
}

std::expected<Json, StarException> Item::instance_values() const {
  auto parameters = m_parameters.to_object();
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  return m_config.set_all(std::move(*parameters));
}

Json Item::config() const {
  return m_config;
}

Json Item::parameters() const {
  return m_parameters;
}

std::expected<void, StarException> Item::set_instance_value(String const& name, Json const& value) {
  auto current = m_parameters.get(name, {});
  if (!current)
    return std::unexpected(std::move(current.error()));
  if (*current != value) {
    auto parameters = m_parameters.set(name, value);
    if (!parameters)
      return std::unexpected(std::move(parameters.error()));
    m_parameters = std::move(*parameters);
  }
  return {};
}

String const& Item::directory() const& {
  return m_directory;
}

List<ItemDescriptor> const& Item::learn_blueprints_on_pickup() const& {
  return m_learn_blueprints_on_pickup;
}

StringMap<String> const& Item::collectables_on_pickup() const& {
  return m_collectables_on_pickup;
}

std::expected<std::shared_ptr<GenericItem>, StarException> GenericItem::create(Root& root, Json const& config, String const& directory, Json const& parameters) {
  auto item = std::make_shared<GenericItem>(ConstructionKey{}, root);
  auto initialized = item->initialize(config, directory, parameters);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return item;
}

std::expected<std::shared_ptr<Item>, StarException> GenericItem::clone() const {
  return std::make_shared<GenericItem>(*this);
}

bool Item::items_equal(std::optional<std::reference_wrapper<Item const>> a, std::optional<std::reference_wrapper<Item const>> b) {
  if (!a && !b) // Both are null
    return true;
  if (a && b) // Both aren't null, compare
    return a->get().stackable_with(b);
  else // One is null, so not equal
    return false;
}

}
