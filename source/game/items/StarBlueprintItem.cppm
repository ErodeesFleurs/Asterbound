module;
import std;
import star.format;
import star.type_identity;
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
import star.color;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.a_star;
import star.set;
import star.string;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.version;
import star.ordered_set;
import star.audio;
import star.map;
import star.either;
import star.random;
import star.periodic_function;


import star.drawable;
import star.item;
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
import star.animation;
import star.particle;
import star.physics_entity;
import star.movement_controller;
import star.platformer_astar_types;
import star.anchorable_entity;
import star.game_timers;
import star.actor_movement_controller;
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.tool_user_item;
import star.status_effect_item;
import star.lua_components;
import star.fireable_item;
import star.swingable_item;

import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.uuid;
import star.animated_part_set;
import star.mixer;
import star.networked_animator;
import star.humanoid;
import star.lounging_entities;
import star.chat_action;
import star.chatty_entity;
import star.emote_entity;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.inspectable_entity;
import star.inventory_types;
import star.ai_types;
import star.entity_rendering_types;
import star.entity_rendering;
import star.player_types;
import star.lua_actor_movement_component;
import star.lua_animation_component;
import star.radio_message_database;
import star.player;

export module star.blueprint_item;

export namespace Star {

class BlueprintItem : public Item, public SwingableItem {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<BlueprintItem, Item, SwingableItem>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<BlueprintItem>();
  }

  [[nodiscard]]
  static std::expected<std::shared_ptr<BlueprintItem>, StarException> create(Root& root, Json const& config, String const& directory, Json const& data);
  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> clone() const override;

  [[nodiscard]]
  std::expected<List<Drawable>, StarException> drawables() const override;

  [[nodiscard]]
  std::expected<void, StarException> fireTriggered() override;

  [[nodiscard]]
  std::expected<List<Drawable>, StarException> iconDrawables() const override;
  [[nodiscard]]
  std::expected<List<Drawable>, StarException> dropDrawables(std::optional<std::reference_wrapper<TileDrawer>>) const override;

private:
  explicit BlueprintItem(Root& root) : Item(root) {}
  ItemDescriptor m_recipe;
  Drawable m_recipeIconUnderlay;
  List<Drawable> m_inHandDrawable;
};

}

namespace Star {

std::expected<std::shared_ptr<BlueprintItem>, StarException> BlueprintItem::create(Root& root, Json const& config, String const& directory, Json const& data) {
  auto item = std::shared_ptr<BlueprintItem>(new BlueprintItem(root));
  auto initialized = item->initialize(config, directory, data);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto swingable = item->initializeSwingable(config);
  if (!swingable)
    return std::unexpected(std::move(swingable.error()));
  item->setWindupTime(0.2f);
  item->setCooldownTime(0.1f);
  item->m_requireEdgeTrigger = true;
  auto recipeValue = item->instanceValue("recipe");
  if (!recipeValue)
    return std::unexpected(std::move(recipeValue.error()));
  auto recipe = ItemDescriptor::create(*recipeValue);
  if (!recipe)
    return std::unexpected(std::move(recipe.error()));
  item->m_recipe = std::move(*recipe);
  auto metadata = root.imageMetadataDatabase();
  if (!metadata)
    return std::unexpected(std::move(metadata.error()));
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto underlayValue = (*assets)->json("/blueprint.config:iconUnderlay");
  if (!underlayValue)
    return std::unexpected(std::move(underlayValue.error()));
  auto underlay = Drawable::create(**metadata, *underlayValue);
  if (!underlay)
    return std::unexpected(std::move(underlay.error()));
  item->m_recipeIconUnderlay = std::move(*underlay);
  auto image = (*assets)->json("/blueprint.config:inHandImage").and_then([](Json const& value) { return value.toString(); });
  if (!image)
    return std::unexpected(std::move(image.error()));
  auto drawable = Drawable::makeImage(**metadata, *image, 1.0f / TilePixels, true, Vec2F());
  if (!drawable)
    return std::unexpected(std::move(drawable.error()));
  item->m_inHandDrawable = {std::move(*drawable)};
  auto factor = (*assets)->json("/items/defaultParameters.config:blueprintPriceFactor").and_then([](Json const& value) { return value.toFloat(); });
  if (!factor)
    return std::unexpected(std::move(factor.error()));
  item->setPrice(int(item->price() * *factor));
  return item;
}

std::expected<std::shared_ptr<Item>, StarException> BlueprintItem::clone() const {
  return std::make_shared<BlueprintItem>(*this);
}

std::expected<List<Drawable>, StarException> BlueprintItem::drawables() const {
  return m_inHandDrawable;
}

std::expected<void, StarException> BlueprintItem::fireTriggered() {
  if (count()) {
    auto itemOwner = owner();
    if (!itemOwner)
      return std::unexpected(std::move(itemOwner.error()));
    if (auto player = asRef<Player>(itemOwner->get())) {
      auto learned = player->get().addBlueprint(m_recipe, true);
      if (!learned)
        return std::unexpected(std::move(learned.error()));
      if (*learned)
        setCount(count() - 1);
    }
  }
  return {};
}

std::expected<List<Drawable>, StarException> BlueprintItem::iconDrawables() const {
  List<Drawable> result;
  result.push_back(m_recipeIconUnderlay);
  auto icons = Item::iconDrawables();
  if (!icons)
    return std::unexpected(std::move(icons.error()));
  result.append_range(std::move(*icons));
  return result;
}

std::expected<List<Drawable>, StarException> BlueprintItem::dropDrawables(std::optional<std::reference_wrapper<TileDrawer>>) const {
  return m_inHandDrawable;
}

}
