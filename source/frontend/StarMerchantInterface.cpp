import std;
import star.memory;
import star.json;
import star.xx_hash;
import star.json_extra;
import star.id_map;
import star.ordered_map;
import star.lru_cache;
import star.thread;
import star.time;
import star.random;
import star.ttl_cache;
import star.casting;
import star.game_types;
import star.strong_typedef;
import star.rpc_promise;
import star.vector;
import star.bi_map;
import star.a_star;
import star.data_stream;
import star.input_event;
import star.color;
import star.font;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.string;
import star.text;
import star.asset_path;
import star.listener;
import star.set;
import star.audio;
import star.map;
import star.lexical_cast;
import star.image;
import star.math_common;
import star.format;
import star.weighted_pool;
import star.net_compatibility;
import star.worker_pool;

import star.sector_array_2d;
import star.perlin;
import star.btree;
import star.data_stream_devices;
import star.btree_database;
import star.ordered_set;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.periodic_function;

import star.lua_root;

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.tile_damage;
import star.interaction_types;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_modification;
import star.force_regions;
import star.world;
import star.physics_entity;
import star.movement_controller;
import star.platformer_astar_types;
import star.anchorable_entity;

import star.game_timers;
import star.actor_movement_controller;
import star.item;
import star.item_descriptor;
import star.item_database;
import star.item_recipe;

import star.liquid_types;
import star.tile_sector_array;
import star.particle;
import star.weather_types;
import star.sky_types;
import star.world_parameters;
import star.celestial_parameters;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.celestial_types;
import star.chat_types;
import star.uuid;
import star.warping;
import star.wiring;
import star.inspectable_entity;
import star.plant;
import star.plant_database;
import star.biome_placement;
import star.versioning_database;
import star.world_storage;
import star.player_types;
import star.sky_parameters;
import star.system_world;
import star.damage_manager;
import star.net_packets;
import star.entity_rendering_types;
import star.sky_render_data;
import star.parallax;
import star.cellular_light_array;
import star.cellular_lighting;
import star.world_render_data;
import star.world_structure;
import star.chat_action;
import star.entity_rendering;
import star.lua_components;
import star.world_client_state;
import star.interpolation_tracker;
import star.ambient;
import star.weather;
import star.world_client;
import star.application;
import star.statistics_service;
import star.p2p_networking_service;
import star.user_generated_content_service;
import star.desktop_service;
import star.application_controller;
import star.renderer;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.font_texture_group;
import star.anchor_types;
import star.text_painter;
import star.drawable;
import star.asset_texture_group;
import star.drawable_painter;
import star.gui_types;
import star.key_bindings;
import star.mixer;
import star.gui_context;
import star.widget;
import star.pane;


import star.merchant_interface;
import star.widget_parsing;
import star.gui_reader;
import star.animated_part_set;
import star.networked_animator;
import star.humanoid;
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.lounging_entities;
import star.chatty_entity;
import star.emote_entity;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.inventory_types;
import star.ai_types;
import star.lua_actor_movement_component;
import star.lua_animation_component;
import star.radio_message_database;
import star.player;
import star.button_group;
import star.button_widget;
import star.label_widget;
import star.text_box_widget;
import star.image_widget;
import star.progress_widget;
import star.animation;
import star.item_slot_widget;
import star.item_grid_widget;
import star.list_widget;
import star.tab_set;

import star.item_tooltip;


import star.player_inventory;
import star.item_bag;

import star.quest_manager;

import star.algorithm;
namespace Star {

namespace {

// Both counts and totals must remain positive integers in the onBuy JSON.
constexpr auto max_purchase_value = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());

std::uint64_t purchase_total(std::uint64_t per_bundle, int bundles) {
  if (bundles <= 0 || per_bundle > max_purchase_value / static_cast<std::uint64_t>(bundles))
    return 0;
  return per_bundle * static_cast<std::uint64_t>(bundles);
}

}

MerchantPane::MerchantPane(GuiContext& guiContext) : Pane(guiContext) {}

std::expected<std::shared_ptr<MerchantPane>, StarException> MerchantPane::create(GuiContext& guiContext,
    std::shared_ptr<WorldClient> worldClient, std::shared_ptr<Player> player, Json const& settings, EntityId sourceEntityId) {
  auto pane = std::shared_ptr<MerchantPane>(new MerchantPane(guiContext));
  auto base = pane->initializePane();
  if (!base)
    return std::unexpected(std::move(base.error()));
  auto initialized = pane->initialize(std::move(worldClient), std::move(player), settings, sourceEntityId);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return pane;
}

std::expected<void, StarException> MerchantPane::initialize(std::shared_ptr<WorldClient> worldClient, std::shared_ptr<Player> player, Json const& settings, EntityId sourceEntityId) {
  m_worldClient = std::move(worldClient);
  m_player = std::move(player);
  m_sourceEntityId = sourceEntityId;

  auto assetsResult568 = context().root().assets();
  if (!assetsResult568)
    return std::unexpected(std::move(assetsResult568.error()));
  auto assets = std::move(*assetsResult568);
  auto getResult569 = settings.get("config", "/interface/windowconfig/merchant.config");
  if (!getResult569)
    return std::unexpected(std::move(getResult569.error()));
  auto baseConfig = std::move(*getResult569);
  auto fetchJsonResult570 = assets->fetch_json(baseConfig);
  if (!fetchJsonResult570)
    return std::unexpected(std::move(fetchJsonResult570.error()));
  m_settings = json_merge(std::move(*fetchJsonResult570), settings);

  auto refresh_config = assets->json("/merchant.config:autoRefreshRate");
  if (!refresh_config)
    return std::unexpected(std::move(refresh_config.error()));
  auto refresh_duration = std::move(*refresh_config).to_float();
  if (!refresh_duration)
    return std::unexpected(std::move(refresh_duration.error()));
  m_refresh_timer = GameTimer(std::move(*refresh_duration));

  auto jsonResult573 = assets->json("/merchant.config:defaultBuyFactor");
  if (!jsonResult573)
    return std::unexpected(std::move(jsonResult573.error()));
  auto toFloatResult574 = std::move(*jsonResult573).to_float();
  if (!toFloatResult574)
    return std::unexpected(std::move(toFloatResult574.error()));
  auto getFloatResult575 = m_settings.get_float("buyFactor", std::move(*toFloatResult574));
  if (!getFloatResult575)
    return std::unexpected(std::move(getFloatResult575.error()));
  m_buyFactor = std::move(*getFloatResult575);
  auto jsonResult576 = assets->json("/merchant.config:defaultSellFactor");
  if (!jsonResult576)
    return std::unexpected(std::move(jsonResult576.error()));
  auto toFloatResult577 = std::move(*jsonResult576).to_float();
  if (!toFloatResult577)
    return std::unexpected(std::move(toFloatResult577.error()));
  auto getFloatResult578 = m_settings.get_float("sellFactor", std::move(*toFloatResult577));
  if (!getFloatResult578)
    return std::unexpected(std::move(getFloatResult578.error()));
  m_sellFactor = std::move(*getFloatResult578);

  auto getUIntResult579 = m_settings.get_uint("sellContainerSize");
  if (!getUIntResult579)
    return std::unexpected(std::move(getUIntResult579.error()));
  m_itemBag = std::make_shared<ItemBag>(context().root(), std::move(*getUIntResult579));

  auto jsonResult580 = assets->json("/interface/windowconfig/crafting.config:default");
  if (!jsonResult580)
    return std::unexpected(std::move(jsonResult580.error()));
  auto default_max_buy_count = std::move(*jsonResult580).get_int("maxSpinCount", 1000);
  if (!default_max_buy_count)
    return std::unexpected(std::move(default_max_buy_count.error()));
  auto max_buy_count_setting = m_settings.get_int("maxSpinCount", *default_max_buy_count);
  if (!max_buy_count_setting)
    return std::unexpected(std::move(max_buy_count_setting.error()));
  m_max_buy_count = static_cast<int>(std::clamp<std::int64_t>(*max_buy_count_setting, 0, std::numeric_limits<int>::max()));

  GuiReader reader(context());
  reader.registerCallback("spinCount.up", [this](Widget&) -> std::expected<void, StarException> {
    auto maximum = max_buy_count();
    if (!maximum)
      return std::unexpected(std::move(maximum.error()));
    if (m_selectedIndex == npos || m_max_buy_count == 0)
      m_buy_count = 0;
    else if (m_buy_count < *maximum)
      ++m_buy_count;
    else
      m_buy_count = 1;
    return count_changed();
  });

  reader.registerCallback("spinCount.down", [this](Widget&) -> std::expected<void, StarException> {
    if (m_selectedIndex == npos || m_max_buy_count == 0) {
      m_buy_count = 0;
    } else if (m_buy_count > 1) {
      --m_buy_count;
    } else {
      auto maximum = max_buy_count();
      if (!maximum)
        return std::unexpected(std::move(maximum.error()));
      m_buy_count = std::max(*maximum, 1);
    }
    return count_changed();
  });

  reader.registerCallback("countChanged", [this](Widget&) { return count_changed(); });
  reader.registerCallback("parseCountText", [this](Widget&) { return count_text_changed(); });
  reader.registerCallback("buy", [this](Widget&) { return buy(); });

  reader.registerCallback("sell", [this](Widget&)  -> std::expected<void, StarException> { auto sellResult590 = sell();
                                                    if (!sellResult590)
                                                      return std::unexpected(std::move(sellResult590.error())); 
  return {};
});

  reader.registerCallback("close", [this](Widget&)  -> std::expected<void, StarException> { dismiss();
  return {};
});

  reader.registerCallback("itemGrid",
      [this](Widget&)  -> std::expected<void, StarException> {
        auto swapSlotResult591 = swapSlot();
        if (!swapSlotResult591)
          return std::unexpected(std::move(swapSlotResult591.error()));
        auto updateSellTotalResult592 = updateSellTotal();
        if (!updateSellTotalResult592)
          return std::unexpected(std::move(updateSellTotalResult592.error()));
      
  return {};
});

  auto getResult593 = m_settings.get("paneLayout");
  if (!getResult593)
    return std::unexpected(std::move(getResult593.error()));
  Json paneLayout = std::move(*getResult593);
  auto getResult594 = m_settings.get("paneLayoutOverride", {});
  if (!getResult594)
    return std::unexpected(std::move(getResult594.error()));
  paneLayout = json_merge(paneLayout, std::move(*getResult594));
  auto constructResult595 = reader.construct(paneLayout, *this);
  if (!constructResult595)
    return std::unexpected(std::move(constructResult595.error()));

  auto findChildResult596 = findChild<TabSetWidget>("buySellTabs");
  if (!findChildResult596)
    return std::unexpected(std::move(findChildResult596.error()));
  m_tabSet = std::move(*findChildResult596);
  m_tabSet->setCallback([this](Widget&)  -> std::expected<void, StarException> {
    auto bgResult = getBG();
    if (m_tabSet->selectedTab() == 0)
      {
        auto getStringResult597 = m_settings.get_string("buyBody");
        if (!getStringResult597)
          return std::unexpected(std::move(getStringResult597.error()));
        bgResult.body = std::move(*getStringResult597);
      }
    else
      {
        auto getStringResult598 = m_settings.get_string("sellBody");
        if (!getStringResult598)
          return std::unexpected(std::move(getStringResult598.error()));
        bgResult.body = std::move(*getStringResult598);
      }
    auto setBGResult599 = setBG(bgResult);
    if (!setBGResult599)
      return std::unexpected(std::move(setBGResult599.error()));
  
  return {};
});
  auto findChildResult600 = findChild<ListWidget>("itemList");
  if (!findChildResult600)
    return std::unexpected(std::move(findChildResult600.error()));
  m_itemGuiList = std::move(*findChildResult600);
  auto findChildResult601 = findChild<TextBoxWidget>("tbCount");
  if (!findChildResult601)
    return std::unexpected(std::move(findChildResult601.error()));
  m_countTextBox = std::move(*findChildResult601);
  auto findChildResult602 = findChild<LabelWidget>("lblBuyTotal");
  if (!findChildResult602)
    return std::unexpected(std::move(findChildResult602.error()));
  m_buyTotalLabel = std::move(*findChildResult602);
  auto findChildResult603 = findChild<ButtonWidget>("btnBuy");
  if (!findChildResult603)
    return std::unexpected(std::move(findChildResult603.error()));
  m_buyButton = std::move(*findChildResult603);
  m_buyButton->disable();
  auto findChildResult604 = findChild<LabelWidget>("lblSellTotal");
  if (!findChildResult604)
    return std::unexpected(std::move(findChildResult604.error()));
  m_sellTotalLabel = std::move(*findChildResult604);
  auto findChildResult605 = findChild<ButtonWidget>("btnSell");
  if (!findChildResult605)
    return std::unexpected(std::move(findChildResult605.error()));
  m_sellButton = std::move(*findChildResult605);

  auto findChildResult606 = findChild<ItemGridWidget>("itemGrid");
  if (!findChildResult606)
    return std::unexpected(std::move(findChildResult606.error()));
  m_itemGrid = std::move(*findChildResult606);
  auto setItemBagResult607 = m_itemGrid->setItemBag(m_itemBag);
  if (!setItemBagResult607)
    return std::unexpected(std::move(setItemBagResult607.error()));

  auto buildItemListResult608 = buildItemList();
  if (!buildItemListResult608)
    return std::unexpected(std::move(buildItemListResult608.error()));

  auto updateSelectionResult609 = updateSelection();
  if (!updateSelectionResult609)
    return std::unexpected(std::move(updateSelectionResult609.error()));

  auto updateSellTotalResult610 = updateSellTotal();
  if (!updateSellTotalResult610)
    return std::unexpected(std::move(updateSellTotalResult610.error()));

return {};
}

std::expected<void, StarException> MerchantPane::displayed() {
  auto displayedResult611 = Pane::displayed();
  if (!displayedResult611)
    return std::unexpected(std::move(displayedResult611.error()));

return {};
}

std::expected<void, StarException> MerchantPane::dismissed() {
  auto dismissedResult612 = Pane::dismissed();
  if (!dismissedResult612)
    return std::unexpected(std::move(dismissedResult612.error()));

  for (auto unsold : m_itemBag->take_all())
    {
      auto giveItemResult613 = m_player->giveItem(unsold);
      if (!giveItemResult613)
        return std::unexpected(std::move(giveItemResult613.error()));
      
    }

  if (m_sourceEntityId != null_entity_id)
    {
      auto sendEntityMessageResult614 = m_worldClient->sendEntityMessage(m_sourceEntityId, "onMerchantClosed");
      if (!sendEntityMessageResult614)
        return std::unexpected(std::move(sendEntityMessageResult614.error()));
      
    }

return {};
}

std::expected<std::shared_ptr<Pane>, StarException> MerchantPane::createTooltip(Vec2I const& screenPosition) {
  if (m_tabSet->selectedTab() == 0) {
    for (std::size_t i = 0; i < m_itemGuiList->numChildren(); ++i) {
      auto entry = m_itemGuiList->itemAt(i);
      if (entry->getChildAt(screenPosition)) {
        auto itemConfig = m_itemList.get(i);
        auto itemDatabaseResult615 = context().root().itemDatabase();
        if (!itemDatabaseResult615)
          return std::unexpected(std::move(itemDatabaseResult615.error()));
        auto getResult616 = itemConfig.get("item");
        if (!getResult616)
          return std::unexpected(std::move(getResult616.error()));
        auto createResult617 = ItemDescriptor::create(std::move(*getResult616));
        if (!createResult617)
          return std::unexpected(std::move(createResult617.error()));
        auto item_shared_result618 = std::move(*itemDatabaseResult615)->item_shared(std::move(*createResult617));
        if (!item_shared_result618)
          return std::unexpected(std::move(item_shared_result618.error()));
        std::shared_ptr<Item> item = std::move(*item_shared_result618);
        auto buildItemTooltipResult619 = ItemTooltipBuilder::buildItemTooltip(context(), item, m_player);
        if (!buildItemTooltipResult619)
          return std::unexpected(std::move(buildItemTooltipResult619.error()));
        return std::move(*buildItemTooltipResult619);
      }
    }
  } else {
    {
      auto itemAtResult620 = m_itemGrid->itemAt(screenPosition);
      if (!itemAtResult620)
        return std::unexpected(std::move(itemAtResult620.error()));
      if (auto item = std::move(*itemAtResult620))
        {
          auto buildItemTooltipResult621 = ItemTooltipBuilder::buildItemTooltip(context(), item, m_player);
          if (!buildItemTooltipResult621)
            return std::unexpected(std::move(buildItemTooltipResult621.error()));
          return std::move(*buildItemTooltipResult621);
        }
    }
  }
  return {};
}

std::expected<void, StarException> MerchantPane::update(float dt) {
  auto updateResult622 = Pane::update(dt);
  if (!updateResult622)
    return std::unexpected(std::move(updateResult622.error()));

  {
    bool condition624 = static_cast<bool>(m_sourceEntityId != null_entity_id);
    if (condition624) {
      auto playerCanReachEntityResult623 = m_worldClient->playerCanReachEntity(m_sourceEntityId);
      if (!playerCanReachEntityResult623)
        return std::unexpected(std::move(playerCanReachEntityResult623.error()));
      condition624 = static_cast<bool>(!std::move(*playerCanReachEntityResult623));
    }
    if (condition624)
      dismiss();
  }

  if (m_refresh_timer.wrap_tick(context().root().timing().time_step)) {
    for (std::size_t i = 0; i < m_itemList.size(); ++i) {
      auto itemConfig = m_itemList.get(i);
      auto itemWidget = m_itemGuiList->itemAt(i);
      auto setupWidgetResult625 = setupWidget(itemWidget, itemConfig);
      if (!setupWidgetResult625)
        return std::unexpected(std::move(setupWidgetResult625.error()));
    }
    if (auto updated = update_buy_total(); !updated)
      return std::unexpected(std::move(updated.error()));
  }

  auto updateSelectionResult627 = updateSelection();
  if (!updateSelectionResult627)
    return std::unexpected(std::move(updateSelectionResult627.error()));

  auto updateAllItemSlotsResult628 = m_itemGrid->updateAllItemSlots();
  if (!updateAllItemSlotsResult628)
    return std::unexpected(std::move(updateAllItemSlotsResult628.error()));

return {};
}

EntityId MerchantPane::sourceEntityId() const {
  return m_sourceEntityId;
}

std::expected<std::shared_ptr<Item>, StarException> MerchantPane::addItems(std::shared_ptr<Item> const& items) {
  if (m_tabSet->selectedTab() == 1) {
    auto addItemsResult629 = m_itemBag->add_items(items);
    if (!addItemsResult629)
      return std::unexpected(std::move(addItemsResult629.error()));
    auto remainder = std::move(*addItemsResult629);
    auto updateSellTotalResult630 = updateSellTotal();
    if (!updateSellTotalResult630)
      return std::unexpected(std::move(updateSellTotalResult630.error()));
    return remainder;
  } else {
    return items;
  }
}

std::expected<void, StarException> MerchantPane::swapSlot() {
  std::shared_ptr<Item> source = m_player->inventory()->swapSlotItem();
  auto inv = m_player->inventory();
  if (context().shiftHeld()) {
    {
      auto selectedItemResult631 = m_itemGrid->selectedItem();
      if (!selectedItemResult631)
        return std::unexpected(std::move(selectedItemResult631.error()));
      if (std::move(*selectedItemResult631)) {
        auto takeItemsResult632 = m_itemBag->take_items(m_itemGrid->selectedIndex());
        if (!takeItemsResult632)
          return std::unexpected(std::move(takeItemsResult632.error()));
        auto addItemsResult633 = inv->addItems(std::move(*takeItemsResult632));
        if (!addItemsResult633)
          return std::unexpected(std::move(addItemsResult633.error()));
        auto remainder = std::move(*addItemsResult633);
        if (remainder && !remainder->empty())
          {
            auto setItemResult634 = m_itemBag->set_item(m_itemGrid->selectedIndex(), remainder);
            if (!setItemResult634)
              return std::unexpected(std::move(setItemResult634.error()));
            
          }
      }
    }
  } else {
    if (auto heldItem = m_player->inventory()->swapSlotItem())
      {
        auto swapItemsResult635 = m_itemBag->swap_items(m_itemGrid->selectedIndex(), heldItem);
        if (!swapItemsResult635)
          return std::unexpected(std::move(swapItemsResult635.error()));
        auto setSwapSlotItemResult636 = inv->setSwapSlotItem(std::move(*swapItemsResult635));
        if (!setSwapSlotItemResult636)
          return std::unexpected(std::move(setSwapSlotItemResult636.error()));
        
      }
    else
      {
        auto takeItemsResult637 = m_itemBag->take_items(m_itemGrid->selectedIndex());
        if (!takeItemsResult637)
          return std::unexpected(std::move(takeItemsResult637.error()));
        auto setSwapSlotItemResult638 = inv->setSwapSlotItem(std::move(*takeItemsResult637));
        if (!setSwapSlotItemResult638)
          return std::unexpected(std::move(setSwapSlotItemResult638.error()));
        
      }
  }
  return {};
}

std::expected<void, StarException> MerchantPane::buildItemList() {
  auto clearResult639 = m_itemGuiList->clear();
  if (!clearResult639)
    return std::unexpected(std::move(clearResult639.error()));
  auto getArrayResult640 = m_settings.get_array("items");
  if (!getArrayResult640)
    return std::unexpected(std::move(getArrayResult640.error()));
  m_itemList = std::move(*getArrayResult640);

  auto itemDatabaseResult641 = context().root().itemDatabase();
  if (!itemDatabaseResult641)
    return std::unexpected(std::move(itemDatabaseResult641.error()));
  auto itemDatabase = std::move(*itemDatabaseResult641);
  JsonArray availableItems;
  for (auto const& itemConfig : m_itemList) {
    auto available = [&]() -> std::expected<bool, StarException> {
      {
        auto getResult642 = itemConfig.get("item");
        if (!getResult642)
          return std::unexpected(std::move(getResult642.error()));
        auto createResult643 = ItemDescriptor::create(std::move(*getResult642));
        if (!createResult643)
          return std::unexpected(std::move(createResult643.error()));
        if (!itemDatabase->has_item(std::move(*createResult643).name()))
          return false;
      }

      {
        auto optStringResult644 = itemConfig.opt_string("prerequisiteQuest");
        if (!optStringResult644)
          return std::unexpected(std::move(optStringResult644.error()));
        if (auto prerequisite = std::move(*optStringResult644)) {
          if (!m_player->questManager()->hasCompleted(*prerequisite))
            return false;
        }
      }

      {
        auto optArrayResult645 = itemConfig.opt_array("exclusiveQuests");
        if (!optArrayResult645)
          return std::unexpected(std::move(optArrayResult645.error()));
        if (auto quests = std::move(*optArrayResult645)) {
          for (auto const& quest : *quests) {
            {
              auto toStringResult646 = quest.to_string();
              if (!toStringResult646)
                return std::unexpected(std::move(toStringResult646.error()));
              if (m_player->questManager()->hasQuest(std::move(*toStringResult646)))
                return false;
            }
          }
        }
      }

      {
        auto optUIntResult647 = itemConfig.opt_uint("prerequisiteShipLevel");
        if (!optUIntResult647)
          return std::unexpected(std::move(optUIntResult647.error()));
        if (auto prerequisite = std::move(*optUIntResult647)) {
          if (m_player->ship_upgrades().ship_level < *prerequisite)
            return false;
        }
      }

      {
        auto optUIntResult648 = itemConfig.opt_uint("maxShipLevel");
        if (!optUIntResult648)
          return std::unexpected(std::move(optUIntResult648.error()));
        if (auto maxLevel = std::move(*optUIntResult648)) {
          if (m_player->ship_upgrades().ship_level > *maxLevel)
            return false;
        }
      }

      return true;
    }();
    if (!available)
      return std::unexpected(std::move(available.error()));
    if (*available)
      availableItems.push_back(itemConfig);
  }
  m_itemList = std::move(availableItems);

  for (auto itemConfig : m_itemList) {
    auto addItemResult649 = m_itemGuiList->addItem();
    if (!addItemResult649)
      return std::unexpected(std::move(addItemResult649.error()));
    auto widget = std::move(*addItemResult649);
    auto setupWidgetResult650 = setupWidget(widget, itemConfig);
    if (!setupWidgetResult650)
      return std::unexpected(std::move(setupWidgetResult650.error()));
  }
  return {};
}

std::expected<void, StarException> MerchantPane::setupWidget(std::shared_ptr<Widget> const& widget, Json const& itemConfig) {
  auto& root = context().root();
  auto assetsResult651 = root.assets();
  if (!assetsResult651)
    return std::unexpected(std::move(assetsResult651.error()));
  auto assets = std::move(*assetsResult651);
  auto itemDatabaseResult652 = root.itemDatabase();
  if (!itemDatabaseResult652)
    return std::unexpected(std::move(itemDatabaseResult652.error()));
  auto getResult653 = itemConfig.get("item");
  if (!getResult653)
    return std::unexpected(std::move(getResult653.error()));
  auto createResult654 = ItemDescriptor::create(std::move(*getResult653));
  if (!createResult654)
    return std::unexpected(std::move(createResult654.error()));
  auto item_shared_result655 = std::move(*itemDatabaseResult652)->item_shared(std::move(*createResult654));
  if (!item_shared_result655)
    return std::unexpected(std::move(item_shared_result655.error()));
  std::shared_ptr<Item> item = std::move(*item_shared_result655);

  String name = item->friendly_name();
  if (item->count() > 1)
    name = Star::format("{} (x{})", name, item->count());

  auto fetchChildResult656 = widget->fetchChild<LabelWidget>("itemName");
  if (!fetchChildResult656)
    return std::unexpected(std::move(fetchChildResult656.error()));
  auto itemName = std::move(*fetchChildResult656);
  auto setTextResult657 = itemName->setText(name);
  if (!setTextResult657)
    return std::unexpected(std::move(setTextResult657.error()));

  auto bundle_price = itemConfig.get_int("price", item->price());
  if (!bundle_price)
    return std::unexpected(std::move(bundle_price.error()));
  std::uint64_t price = 0;
  if (*bundle_price > 0) {
    if (m_buyFactor == 1.0f) {
      price = static_cast<std::uint64_t>(*bundle_price);
    } else {
      auto scaled_price = std::ceil(*bundle_price * m_buyFactor);
      if (is_finite(scaled_price) && scaled_price > 0.0f && scaled_price < 0x1p63f)
        price = static_cast<std::uint64_t>(scaled_price);
    }
  }
  auto setLabelResult659 = widget->setLabel("priceLabel", Star::to_string(price));
  if (!setLabelResult659)
    return std::unexpected(std::move(setLabelResult659.error()));
  widget->setData(price);

  bool unavailable = price == 0 || price > m_player->currency("money") || item->empty() || item->max_stack() == 0;
  auto fetchChildResult660 = widget->fetchChild<ImageWidget>("unavailableoverlay");
  if (!fetchChildResult660)
    return std::unexpected(std::move(fetchChildResult660.error()));
  auto unavailableoverlay = std::move(*fetchChildResult660);
  if (unavailable) {
    itemName->setColor(Color::palette_gray);
    auto showResult661 = unavailableoverlay->show();
    if (!showResult661)
      return std::unexpected(std::move(showResult661.error()));
  } else {
    itemName->setColor(Color::palette_white);
    auto hideResult662 = unavailableoverlay->hide();
    if (!hideResult662)
      return std::unexpected(std::move(hideResult662.error()));
  }

  auto fetchChildResult663 = widget->fetchChild<ItemSlotWidget>("itemIcon");
  if (!fetchChildResult663)
    return std::unexpected(std::move(fetchChildResult663.error()));
  std::move(*fetchChildResult663)->setItem(item);
  auto showResult664 = widget->show();
  if (!showResult664)
    return std::unexpected(std::move(showResult664.error()));

return {};
}

std::expected<void, StarException> MerchantPane::updateSelection() {
  if (m_selectedIndex != m_itemGuiList->selectedItem()) {
    m_selectedIndex = m_itemGuiList->selectedItem();

    if (m_selectedIndex != npos) {
      auto itemConfig = m_itemList.get(m_selectedIndex);
      auto itemDatabaseResult665 = context().root().itemDatabase();
      if (!itemDatabaseResult665)
        return std::unexpected(std::move(itemDatabaseResult665.error()));
      auto getResult666 = itemConfig.get("item");
      if (!getResult666)
        return std::unexpected(std::move(getResult666.error()));
      auto createResult667 = ItemDescriptor::create(std::move(*getResult666));
      if (!createResult667)
        return std::unexpected(std::move(createResult667.error()));
      auto item_shared_result668 = std::move(*itemDatabaseResult665)->item_shared(std::move(*createResult667));
      if (!item_shared_result668)
        return std::unexpected(std::move(item_shared_result668.error()));
      m_selectedItem = std::move(*item_shared_result668);
      auto findChildResult669 = findChild<ButtonWidget>("spinCount.up");
      if (!findChildResult669)
        return std::unexpected(std::move(findChildResult669.error()));
      std::move(*findChildResult669)->enable();
      auto findChildResult670 = findChild<ButtonWidget>("spinCount.down");
      if (!findChildResult670)
        return std::unexpected(std::move(findChildResult670.error()));
      std::move(*findChildResult670)->enable();
      m_countTextBox->setColor(Color::palette_white);
      m_buy_count = std::min(m_max_buy_count, 1);
    } else {
      auto findChildResult671 = findChild<ButtonWidget>("spinCount.up");
      if (!findChildResult671)
        return std::unexpected(std::move(findChildResult671.error()));
      std::move(*findChildResult671)->disable();
      auto findChildResult672 = findChild<ButtonWidget>("spinCount.down");
      if (!findChildResult672)
        return std::unexpected(std::move(findChildResult672.error()));
      std::move(*findChildResult672)->disable();
      m_countTextBox->setColor(Color::palette_gray);
      m_selectedItem.reset();
      m_buy_count = 0;
    }

    return count_changed();
  }
  return {};
}

std::expected<void, StarException> MerchantPane::update_buy_total() {
  m_buy_total = 0;
  if (auto selected = m_itemGuiList->selectedWidget()) {
    auto bundle_price = selected->data().to_uint();
    if (!bundle_price)
      return std::unexpected(std::move(bundle_price.error()));
    m_buy_total = purchase_total(*bundle_price, m_buy_count);
  }

  auto label = m_buyTotalLabel->setText(Star::to_string(m_buy_total));
  if (!label)
    return std::unexpected(std::move(label.error()));

  bool unaffordable = m_buy_total > m_player->currency("money");
  m_buyTotalLabel->setColor(unaffordable ? Color::palette_red : Color::palette_white);
  m_buyButton->setEnabled(m_buy_total != 0 && !unaffordable && m_selectedItem && m_selectedItem->max_stack() != 0
      && purchase_total(m_selectedItem->count(), m_buy_count) != 0);
  return {};
}

std::expected<void, StarException> MerchantPane::buy() {
  if (!m_selectedItem || m_selectedIndex != m_itemGuiList->selectedItem() || m_buy_total == 0 || m_selectedItem->max_stack() == 0)
    return {};
  auto total_items = purchase_total(m_selectedItem->count(), m_buy_count);
  if (total_items == 0 || !m_player->inventory()->consumeCurrency("money", m_buy_total))
    return {};

  auto remaining_items = total_items;
  while (remaining_items != 0) {
    auto stack = m_selectedItem->clone();
    if (!stack)
      return std::unexpected(std::move(stack.error()));
    if (!*stack)
      return std::unexpected(ItemException("Could not clone merchant purchase item"));
    auto overflow = (*stack)->set_count(remaining_items);
    if (overflow == remaining_items)
      return std::unexpected(ItemException("Merchant purchase item has no stack capacity"));
    if (auto given = m_player->giveItem(*stack); !given)
      return std::unexpected(std::move(given.error()));
    remaining_items = overflow;
  }

  if (m_sourceEntityId != null_entity_id) {
    auto descriptor = m_selectedItem->descriptor().with_count(total_items);
    auto summary = JsonObject{{"item", descriptor.to_json()}, {"total", m_buy_total}};
    auto sent = m_worldClient->sendEntityMessage(m_sourceEntityId, "onBuy", {std::move(summary)});
    if (!sent)
      return std::unexpected(std::move(sent.error()));
  }

  auto& gui_context = context();
  auto assets = gui_context.root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto sound = (*assets)->json("/merchant.config:buySound").and_then([](Json const& value) { return value.to_string(); });
  if (!sound)
    return std::unexpected(std::move(sound.error()));
  if (auto played = gui_context.playAudio(std::move(*sound)); !played)
    return std::unexpected(std::move(played.error()));

  if (auto rebuilt = buildItemList(); !rebuilt)
    return std::unexpected(std::move(rebuilt.error()));
  return updateSelection();
}

std::expected<void, StarException> MerchantPane::updateSellTotal() {
  m_sellTotal = 0;
  for (auto item : m_itemBag->items()) {
    if (item)
      m_sellTotal += std::round(item->price() * m_sellFactor);
  }
  auto setTextResult686 = m_sellTotalLabel->setText(Star::to_string(m_sellTotal));
  if (!setTextResult686)
    return std::unexpected(std::move(setTextResult686.error()));
  if (m_sellTotal > 0)
    m_sellButton->enable();
  else
    m_sellButton->disable();
  return {};
}

std::expected<void, StarException> MerchantPane::sell() {
  if (m_sellTotal > 0) {
    auto toJsonResult687 = m_itemBag->to_json();
    if (!toJsonResult687)
      return std::unexpected(std::move(toJsonResult687.error()));
    auto sellSummary = JsonObject{{"items", std::move(*toJsonResult687)}, {"total", m_sellTotal}};
    auto sendEntityMessageResult688 = m_worldClient->sendEntityMessage(m_sourceEntityId, "onSell", {sellSummary});
    if (!sendEntityMessageResult688)
      return std::unexpected(std::move(sendEntityMessageResult688.error()));

    auto addCurrencyResult689 = m_player->inventory()->addCurrency("money", m_sellTotal);
    if (!addCurrencyResult689)
      return std::unexpected(std::move(addCurrencyResult689.error()));
    m_itemBag->clear_items();
    auto updateSellTotalResult690 = updateSellTotal();
    if (!updateSellTotalResult690)
      return std::unexpected(std::move(updateSellTotalResult690.error()));

    auto& guiContext = context();
    auto assetsResult691 = context().root().assets();
    if (!assetsResult691)
      return std::unexpected(std::move(assetsResult691.error()));
    auto jsonResult692 = std::move(*assetsResult691)->json("/merchant.config:sellSound");
    if (!jsonResult692)
      return std::unexpected(std::move(jsonResult692.error()));
    auto toStringResult693 = std::move(*jsonResult692).to_string();
    if (!toStringResult693)
      return std::unexpected(std::move(toStringResult693.error()));
    auto playAudioResult694 = guiContext.playAudio(std::move(*toStringResult693));
    if (!playAudioResult694)
      return std::unexpected(std::move(playAudioResult694.error()));
  }
  return {};
}

std::expected<int, StarException> MerchantPane::max_buy_count() {
  auto selected = m_itemGuiList->selectedWidget();
  if (!selected)
    return 0;
  auto bundle_price = selected->data().to_uint();
  if (!bundle_price)
    return std::unexpected(std::move(bundle_price.error()));
  if (*bundle_price == 0)
    return m_max_buy_count;
  return static_cast<int>(std::min<std::uint64_t>(m_max_buy_count, m_player->currency("money") / *bundle_price));
}

std::expected<void, StarException> MerchantPane::count_changed() {
  auto text = m_countTextBox->setText(Star::format("x{}", m_buy_count), false);
  if (!text)
    return std::unexpected(std::move(text.error()));
  return update_buy_total();
}

std::expected<void, StarException> MerchantPane::count_text_changed() {
  if (m_selectedIndex == npos || m_max_buy_count == 0) {
    m_buy_count = 0;
    return count_changed();
  }
  auto count = m_countTextBox->getText().replace("x", "");
  if (!count)
    return std::unexpected(std::move(count.error()));
  if (count->empty())
    return {};
  auto parsed = lexical_cast<int>(*count);
  if (!parsed && !parsed.error().is<BadLexicalCast>())
    return std::unexpected(std::move(parsed.error()));
  auto maximum = max_buy_count();
  if (!maximum)
    return std::unexpected(std::move(maximum.error()));
  m_buy_count = std::clamp(parsed.value_or(1), 1, std::max(*maximum, 1));
  return count_changed();
}

}
