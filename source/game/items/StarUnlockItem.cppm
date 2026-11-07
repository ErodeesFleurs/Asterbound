module;
import std;
import star.type_identity;
import star.json;
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
import star.string;
import star.set;
import star.ordered_set;
import star.audio;
import star.map;
import star.either;
import star.random;
import star.periodic_function;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.format;
import star.version;


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
import star.drawable;
import star.previewable_item;

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
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

export module star.unlock_item;

export namespace Star {

class UnlockItem : public Item, public SwingableItem, public PreviewableItem {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<UnlockItem, Item, SwingableItem, PreviewableItem>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<UnlockItem>();
  }

  [[nodiscard]]
  static std::expected<std::shared_ptr<UnlockItem>, StarException> create(Root& root, Json const& config, String const& directory, Json const& itemParameters = JsonObject());

  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> clone() const override;

  [[nodiscard]]
  std::expected<List<Drawable>, StarException> drawables() const override;
  [[nodiscard]]
  std::expected<List<Drawable>, StarException> preview(std::optional<std::reference_wrapper<TileDrawer>>, std::shared_ptr<Player> const& viewer = {}) const override;

protected:
  [[nodiscard]]
  std::expected<void, StarException> fireTriggered() override;

private:
  explicit UnlockItem(Root& root) : Item(root) {}
  std::optional<String> m_sectorUnlock;
  std::optional<String> m_tierRecipesUnlock;
  std::optional<unsigned> m_shipUpgrade;
  String m_unlockMessage;
  List<Drawable> m_drawables;
};

}

namespace Star {

std::expected<std::shared_ptr<UnlockItem>, StarException> UnlockItem::create(Root& root, Json const& config, String const& directory, Json const& itemParameters) {
  auto item = std::shared_ptr<UnlockItem>(new UnlockItem(root));
  auto initialized = item->initialize(config, directory, itemParameters);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto swingable = item->initializeSwingable(config);
  if (!swingable)
    return std::unexpected(std::move(swingable.error()));
  auto recipes = item->instanceValue("tierRecipesUnlock").and_then([](Json const& value) { return value.optString(); });
  if (!recipes)
    return std::unexpected(std::move(recipes.error()));
  item->m_tierRecipesUnlock = std::move(*recipes);
  auto upgrade = item->instanceValue("shipUpgrade").and_then([](Json const& value) { return value.optUInt(); });
  if (!upgrade)
    return std::unexpected(std::move(upgrade.error()));
  item->m_shipUpgrade = *upgrade;
  auto message = item->instanceValue("unlockMessage").and_then([](Json const& value) { return value.optString(); });
  if (!message)
    return std::unexpected(std::move(message.error()));
  item->m_unlockMessage = message->value_or(String{});
  auto image = item->instanceValue("image").and_then([](Json const& value) { return value.toString(); });
  if (!image)
    return std::unexpected(std::move(image.error()));
  auto metadata = root.imageMetadataDatabase();
  if (!metadata)
    return std::unexpected(std::move(metadata.error()));
  auto drawable =
    Drawable::makeImage(**metadata, AssetPath::relative_to(directory, *image), 1.0f / TilePixels, true, Vec2F());
  if (!drawable)
    return std::unexpected(std::move(drawable.error()));
  item->m_drawables = {std::move(*drawable)};
  return item;
}

std::expected<std::shared_ptr<Item>, StarException> UnlockItem::clone() const {
  return std::make_shared<UnlockItem>(*this);
}

std::expected<List<Drawable>, StarException> UnlockItem::drawables() const {
  return m_drawables;
}

std::expected<List<Drawable>, StarException> UnlockItem::preview(std::optional<std::reference_wrapper<TileDrawer>>, std::shared_ptr<Player> const&) const {
  return iconDrawables();
}

std::expected<void, StarException> UnlockItem::fireTriggered() {
  if (!initialized())
    return std::unexpected(ItemException("Item not init'd properly, or user not recognized as Tool User."));
  auto itemOwner = owner();
  if (!itemOwner)
    return std::unexpected(std::move(itemOwner.error()));
  // Only players can use unlock items.
  if (auto player = asRef<Player>(itemOwner->get())) {
    auto shouldConsume = instanceValue("consume", true).and_then([](Json const& value) { return value.toBool(); });
    if (!shouldConsume)
      return std::unexpected(std::move(shouldConsume.error()));
    if (*shouldConsume && !consume(1))
      return {};
    if (auto clientContext = player->get().clientContext()) {
      if (m_shipUpgrade) {
        auto applied = player->get().applyShipUpgrades(JsonObject{{"shipLevel", *m_shipUpgrade}});
        if (!applied)
          return std::unexpected(std::move(applied.error()));
      }
    }
    if (!m_unlockMessage.empty()) {
      JsonObject message;
      message["message"] = m_unlockMessage;
      auto interacted = itemOwner->get().interact(InteractAction(InteractActionType::ShowPopup, itemOwner->get().entityId(), message));
      if (!interacted)
        return std::unexpected(std::move(interacted.error()));
    }
    if (m_tierRecipesUnlock) {
      auto assets = root().assets();
      if (!assets)
        return std::unexpected(std::move(assets.error()));
      auto playerConfig = (*assets)->json("/player.config");
      if (!playerConfig)
        return std::unexpected(std::move(playerConfig.error()));
      List<ItemDescriptor> blueprints;
      auto playerBlueprints = playerConfig->get("defaultBlueprints", JsonObject()).and_then([&](Json const& value) { return value.getArray(*m_tierRecipesUnlock, JsonArray()); });
      if (!playerBlueprints)
        return std::unexpected(std::move(playerBlueprints.error()));
      for (auto const& value : *playerBlueprints) {
        auto blueprint = ItemDescriptor::create(value);
        if (!blueprint)
          return std::unexpected(std::move(blueprint.error()));
        blueprints.append(std::move(*blueprint));
      }
      auto speciesConfig = (*assets)->json(strf("/species/{}.species", player->get().species()));
      if (!speciesConfig)
        return std::unexpected(std::move(speciesConfig.error()));
      auto speciesBlueprints = speciesConfig->get("defaultBlueprints", JsonObject()).and_then([&](Json const& value) { return value.getArray(*m_tierRecipesUnlock, JsonArray()); });
      if (!speciesBlueprints)
        return std::unexpected(std::move(speciesBlueprints.error()));
      for (auto const& value : *speciesBlueprints) {
        auto blueprint = ItemDescriptor::create(value);
        if (!blueprint)
          return std::unexpected(std::move(blueprint.error()));
        blueprints.append(std::move(*blueprint));
      }
      for (auto const& blueprint : blueprints) {
        auto added = player->get().addBlueprint(blueprint);
        if (!added)
          return std::unexpected(std::move(added.error()));
      }
    }
  }
  return {};
}

}
