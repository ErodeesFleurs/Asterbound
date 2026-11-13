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
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.rpc_promise;
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
import star.animation;
import star.particle;
import star.interaction_types;
import star.tile_damage;
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

export module star.codex_item;
import star.player_codexes;
import star.codex;

export namespace Star {

class CodexItem : public Item, public SwingableItem {
public:
  void const* query_type(TypeKey key) const noexcept override {
    return query_type_of<CodexItem, Item, SwingableItem>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<CodexItem>(); }

  [[nodiscard]]
  static std::expected<std::shared_ptr<CodexItem>, StarException> create(Root& root, Json const& config, String const& directory, Json const& data);
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
  explicit CodexItem(Root& root) : Item(root) {}
  String m_codexId;
  List<Drawable> m_iconDrawables;
  List<Drawable> m_worldDrawables;
};

}

namespace Star {

std::expected<std::shared_ptr<CodexItem>, StarException> CodexItem::create(Root& root, Json const& config, String const& directory, Json const& data) {
  auto item = std::shared_ptr<CodexItem>(new CodexItem(root));
  auto initialized = item->initialize(config, directory, data);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto swingable = item->initializeSwingable(config);
  if (!swingable)
    return std::unexpected(std::move(swingable.error()));
  item->setWindupTime(0.2f);
  item->setCooldownTime(0.5f);
  item->m_requireEdgeTrigger = true;
  auto codexId = item->instanceValue("codexId").and_then([](Json const& value) { return value.toString(); });
  if (!codexId)
    return std::unexpected(std::move(codexId.error()));
  item->m_codexId = std::move(*codexId);
  auto iconPath = item->instanceValue("codexIcon").and_then([](Json const& value) { return value.toString(); });
  if (!iconPath)
    return std::unexpected(std::move(iconPath.error()));
  auto metadata = root.imageMetadataDatabase();
  if (!metadata)
    return std::unexpected(std::move(metadata.error()));
  auto icon = Drawable::makeImage(**metadata, *iconPath, 1.0f, true, Vec2F());
  if (!icon)
    return std::unexpected(std::move(icon.error()));
  item->m_iconDrawables = {std::move(*icon)};
  auto worldDrawable = Drawable::makeImage(**metadata, *iconPath, 1.0f / TilePixels, true, Vec2F());
  if (!worldDrawable)
    return std::unexpected(std::move(worldDrawable.error()));
  item->m_worldDrawables = {std::move(*worldDrawable)};
  return item;
}

std::expected<std::shared_ptr<Item>, StarException> CodexItem::clone() const {
  return std::make_shared<CodexItem>(*this);
}

std::expected<List<Drawable>, StarException> CodexItem::drawables() const {
  return m_worldDrawables;
}

std::expected<void, StarException> CodexItem::fireTriggered() {
  auto itemOwner = owner();
  if (!itemOwner)
    return std::unexpected(std::move(itemOwner.error()));
  if (auto player = as_ref<Player>(itemOwner->get())) {
    auto codexLearned = player->get().codexes()->learnCodex(m_codexId);
    if (!codexLearned)
      return std::unexpected(std::move(codexLearned.error()));
    auto assets = root().assets();
    if (!assets)
      return std::unexpected(std::move(assets.error()));
    if (*codexLearned) {
      auto message = (*assets)->json("/codex.config:messages.learned").and_then([](Json const& value) { return value.toString(); });
      if (!message)
        return std::unexpected(std::move(message.error()));
      return player->get().queueUIMessage(*message);
    }
    player->get().codexes()->markCodexUnread(m_codexId);
    auto message = (*assets)->json("/codex.config:messages.alreadyKnown").and_then([](Json const& value) { return value.toString(); });
    if (!message)
      return std::unexpected(std::move(message.error()));
    return player->get().queueUIMessage(*message);
  }
  return {};
}

std::expected<List<Drawable>, StarException> CodexItem::iconDrawables() const {
  return m_iconDrawables;
}

std::expected<List<Drawable>, StarException> CodexItem::dropDrawables(std::optional<std::reference_wrapper<TileDrawer>>) const {
  return m_worldDrawables;
}

}
