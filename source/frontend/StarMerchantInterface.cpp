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
import star.version;
import star.either;
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
  auto fetchJsonResult570 = assets->fetchJson(baseConfig);
  if (!fetchJsonResult570)
    return std::unexpected(std::move(fetchJsonResult570.error()));
  m_settings = jsonMerge(std::move(*fetchJsonResult570), settings);

  auto jsonResult571 = assets->json("/merchant.config:autoRefreshRate");
  if (!jsonResult571)
    return std::unexpected(std::move(jsonResult571.error()));
  auto toFloatResult572 = std::move(*jsonResult571).toFloat();
  if (!toFloatResult572)
    return std::unexpected(std::move(toFloatResult572.error()));
  m_refreshTimer = GameTimer(std::move(*toFloatResult572));

  auto jsonResult573 = assets->json("/merchant.config:defaultBuyFactor");
  if (!jsonResult573)
    return std::unexpected(std::move(jsonResult573.error()));
  auto toFloatResult574 = std::move(*jsonResult573).toFloat();
  if (!toFloatResult574)
    return std::unexpected(std::move(toFloatResult574.error()));
  auto getFloatResult575 = m_settings.getFloat("buyFactor", std::move(*toFloatResult574));
  if (!getFloatResult575)
    return std::unexpected(std::move(getFloatResult575.error()));
  m_buyFactor = std::move(*getFloatResult575);
  auto jsonResult576 = assets->json("/merchant.config:defaultSellFactor");
  if (!jsonResult576)
    return std::unexpected(std::move(jsonResult576.error()));
  auto toFloatResult577 = std::move(*jsonResult576).toFloat();
  if (!toFloatResult577)
    return std::unexpected(std::move(toFloatResult577.error()));
  auto getFloatResult578 = m_settings.getFloat("sellFactor", std::move(*toFloatResult577));
  if (!getFloatResult578)
    return std::unexpected(std::move(getFloatResult578.error()));
  m_sellFactor = std::move(*getFloatResult578);

  auto getUIntResult579 = m_settings.getUInt("sellContainerSize");
  if (!getUIntResult579)
    return std::unexpected(std::move(getUIntResult579.error()));
  m_itemBag = std::make_shared<ItemBag>(context().root(), std::move(*getUIntResult579));

  auto jsonResult580 = assets->json("/interface/windowconfig/crafting.config:default");
  if (!jsonResult580)
    return std::unexpected(std::move(jsonResult580.error()));
  auto getUIntResult581 = std::move(*jsonResult580).getUInt("maxSpinCount", 1000);
  if (!getUIntResult581)
    return std::unexpected(std::move(getUIntResult581.error()));
  auto getUIntResult582 = m_settings.getUInt("maxSpinCount", std::move(*getUIntResult581));
  if (!getUIntResult582)
    return std::unexpected(std::move(getUIntResult582.error()));
  m_maxBuyCount = std::move(*getUIntResult582);

  GuiReader reader(context());
  reader.registerCallback("spinCount.up", [this](Widget&)  -> std::expected<void, StarException> {
      if (m_selectedIndex != NPos) {
        {
          auto maxBuyCountResult583 = maxBuyCount();
          if (!maxBuyCountResult583)
            return std::unexpected(std::move(maxBuyCountResult583.error()));
          if (m_buyCount < std::move(*maxBuyCountResult583))
            m_buyCount++;
          else
            m_buyCount = 1;
        }
      } else {
        m_buyCount = 0;
      }
      auto countChangedResult584 = countChanged();
      if (!countChangedResult584)
        return std::unexpected(std::move(countChangedResult584.error()));
    
  return {};
});

  reader.registerCallback("spinCount.down", [this](Widget&)  -> std::expected<void, StarException> {
      if (m_selectedIndex != NPos) {
        if (m_buyCount > 1)
          m_buyCount--;
        else
          {
            auto maxBuyCountResult585 = maxBuyCount();
            if (!maxBuyCountResult585)
              return std::unexpected(std::move(maxBuyCountResult585.error()));
            m_buyCount = std::max(std::move(*maxBuyCountResult585), 1);
          }
      } else {
        m_buyCount = 0;
      }
      auto countChangedResult586 = countChanged();
      if (!countChangedResult586)
        return std::unexpected(std::move(countChangedResult586.error()));
    
  return {};
});

  reader.registerCallback("countChanged", [this](Widget&)  -> std::expected<void, StarException> { auto countChangedResult587 = countChanged();
                                                            if (!countChangedResult587)
                                                              return std::unexpected(std::move(countChangedResult587.error())); 
  return {};
});
  reader.registerCallback("parseCountText", [this](Widget&)  -> std::expected<void, StarException> { auto countTextChangedResult588 = countTextChanged();
                                                              if (!countTextChangedResult588)
                                                                return std::unexpected(std::move(countTextChangedResult588.error())); 
  return {};
});

  reader.registerCallback("buy", [this](Widget&)  -> std::expected<void, StarException> { auto buyResult589 = buy();
                                                   if (!buyResult589)
                                                     return std::unexpected(std::move(buyResult589.error())); 
  return {};
});

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
  paneLayout = jsonMerge(paneLayout, std::move(*getResult594));
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
        auto getStringResult597 = m_settings.getString("buyBody");
        if (!getStringResult597)
          return std::unexpected(std::move(getStringResult597.error()));
        bgResult.body = std::move(*getStringResult597);
      }
    else
      {
        auto getStringResult598 = m_settings.getString("sellBody");
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

  for (auto unsold : m_itemBag->takeAll())
    {
      auto giveItemResult613 = m_player->giveItem(unsold);
      if (!giveItemResult613)
        return std::unexpected(std::move(giveItemResult613.error()));
      
    }

  if (m_sourceEntityId != NullEntityId)
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
        auto itemSharedResult618 = std::move(*itemDatabaseResult615)->itemShared(std::move(*createResult617));
        if (!itemSharedResult618)
          return std::unexpected(std::move(itemSharedResult618.error()));
        std::shared_ptr<Item> item = std::move(*itemSharedResult618);
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
    bool condition624 = static_cast<bool>(m_sourceEntityId != NullEntityId);
    if (condition624) {
      auto playerCanReachEntityResult623 = m_worldClient->playerCanReachEntity(m_sourceEntityId);
      if (!playerCanReachEntityResult623)
        return std::unexpected(std::move(playerCanReachEntityResult623.error()));
      condition624 = static_cast<bool>(!std::move(*playerCanReachEntityResult623));
    }
    if (condition624)
      dismiss();
  }

  if (m_refreshTimer.wrapTick(context().root().timing().timestep)) {
    for (std::size_t i = 0; i < m_itemList.size(); ++i) {
      auto itemConfig = m_itemList.get(i);
      auto itemWidget = m_itemGuiList->itemAt(i);
      auto setupWidgetResult625 = setupWidget(itemWidget, itemConfig);
      if (!setupWidgetResult625)
        return std::unexpected(std::move(setupWidgetResult625.error()));
    }
    auto updateBuyTotalResult626 = updateBuyTotal();
    if (!updateBuyTotalResult626)
      return std::unexpected(std::move(updateBuyTotalResult626.error()));
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
    auto addItemsResult629 = m_itemBag->addItems(items);
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
        auto takeItemsResult632 = m_itemBag->takeItems(m_itemGrid->selectedIndex());
        if (!takeItemsResult632)
          return std::unexpected(std::move(takeItemsResult632.error()));
        auto addItemsResult633 = inv->addItems(std::move(*takeItemsResult632));
        if (!addItemsResult633)
          return std::unexpected(std::move(addItemsResult633.error()));
        auto remainder = std::move(*addItemsResult633);
        if (remainder && !remainder->empty())
          {
            auto setItemResult634 = m_itemBag->setItem(m_itemGrid->selectedIndex(), remainder);
            if (!setItemResult634)
              return std::unexpected(std::move(setItemResult634.error()));
            
          }
      }
    }
  } else {
    if (auto heldItem = m_player->inventory()->swapSlotItem())
      {
        auto swapItemsResult635 = m_itemBag->swapItems(m_itemGrid->selectedIndex(), heldItem);
        if (!swapItemsResult635)
          return std::unexpected(std::move(swapItemsResult635.error()));
        auto setSwapSlotItemResult636 = inv->setSwapSlotItem(std::move(*swapItemsResult635));
        if (!setSwapSlotItemResult636)
          return std::unexpected(std::move(setSwapSlotItemResult636.error()));
        
      }
    else
      {
        auto takeItemsResult637 = m_itemBag->takeItems(m_itemGrid->selectedIndex());
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
  auto getArrayResult640 = m_settings.getArray("items");
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
        if (!itemDatabase->hasItem(std::move(*createResult643).name()))
          return false;
      }

      {
        auto optStringResult644 = itemConfig.optString("prerequisiteQuest");
        if (!optStringResult644)
          return std::unexpected(std::move(optStringResult644.error()));
        if (auto prerequisite = std::move(*optStringResult644)) {
          if (!m_player->questManager()->hasCompleted(*prerequisite))
            return false;
        }
      }

      {
        auto optArrayResult645 = itemConfig.optArray("exclusiveQuests");
        if (!optArrayResult645)
          return std::unexpected(std::move(optArrayResult645.error()));
        if (auto quests = std::move(*optArrayResult645)) {
          for (auto const& quest : *quests) {
            {
              auto toStringResult646 = quest.toString();
              if (!toStringResult646)
                return std::unexpected(std::move(toStringResult646.error()));
              if (m_player->questManager()->hasQuest(std::move(*toStringResult646)))
                return false;
            }
          }
        }
      }

      {
        auto optUIntResult647 = itemConfig.optUInt("prerequisiteShipLevel");
        if (!optUIntResult647)
          return std::unexpected(std::move(optUIntResult647.error()));
        if (auto prerequisite = std::move(*optUIntResult647)) {
          if (m_player->shipUpgrades().shipLevel < *prerequisite)
            return false;
        }
      }

      {
        auto optUIntResult648 = itemConfig.optUInt("maxShipLevel");
        if (!optUIntResult648)
          return std::unexpected(std::move(optUIntResult648.error()));
        if (auto maxLevel = std::move(*optUIntResult648)) {
          if (m_player->shipUpgrades().shipLevel > *maxLevel)
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
  auto itemSharedResult655 = std::move(*itemDatabaseResult652)->itemShared(std::move(*createResult654));
  if (!itemSharedResult655)
    return std::unexpected(std::move(itemSharedResult655.error()));
  std::shared_ptr<Item> item = std::move(*itemSharedResult655);

  String name = item->friendlyName();
  if (item->count() > 1)
    name = strf("{} (x{})", name, item->count());

  auto fetchChildResult656 = widget->fetchChild<LabelWidget>("itemName");
  if (!fetchChildResult656)
    return std::unexpected(std::move(fetchChildResult656.error()));
  auto itemName = std::move(*fetchChildResult656);
  auto setTextResult657 = itemName->setText(name);
  if (!setTextResult657)
    return std::unexpected(std::move(setTextResult657.error()));

  auto getIntResult658 = itemConfig.getInt("price", item->price());
  if (!getIntResult658)
    return std::unexpected(std::move(getIntResult658.error()));
  unsigned price = std::ceil(std::move(*getIntResult658) * m_buyFactor);
  auto setLabelResult659 = widget->setLabel("priceLabel", toString(price));
  if (!setLabelResult659)
    return std::unexpected(std::move(setLabelResult659.error()));
  widget->setData(price);

  bool unavailable = price > m_player->currency("money");
  auto fetchChildResult660 = widget->fetchChild<ImageWidget>("unavailableoverlay");
  if (!fetchChildResult660)
    return std::unexpected(std::move(fetchChildResult660.error()));
  auto unavailableoverlay = std::move(*fetchChildResult660);
  if (unavailable) {
    itemName->setColor(Color::Gray);
    auto showResult661 = unavailableoverlay->show();
    if (!showResult661)
      return std::unexpected(std::move(showResult661.error()));
  } else {
    itemName->setColor(Color::White);
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

    if (m_selectedIndex != NPos) {
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
      auto itemSharedResult668 = std::move(*itemDatabaseResult665)->itemShared(std::move(*createResult667));
      if (!itemSharedResult668)
        return std::unexpected(std::move(itemSharedResult668.error()));
      m_selectedItem = std::move(*itemSharedResult668);
      auto findChildResult669 = findChild<ButtonWidget>("spinCount.up");
      if (!findChildResult669)
        return std::unexpected(std::move(findChildResult669.error()));
      std::move(*findChildResult669)->enable();
      auto findChildResult670 = findChild<ButtonWidget>("spinCount.down");
      if (!findChildResult670)
        return std::unexpected(std::move(findChildResult670.error()));
      std::move(*findChildResult670)->enable();
      m_countTextBox->setColor(Color::White);
      m_buyCount = 1;
    } else {
      auto findChildResult671 = findChild<ButtonWidget>("spinCount.up");
      if (!findChildResult671)
        return std::unexpected(std::move(findChildResult671.error()));
      std::move(*findChildResult671)->disable();
      auto findChildResult672 = findChild<ButtonWidget>("spinCount.down");
      if (!findChildResult672)
        return std::unexpected(std::move(findChildResult672.error()));
      std::move(*findChildResult672)->disable();
      m_countTextBox->setColor(Color::Gray);
      m_buyCount = 0;
    }

    auto countChangedResult673 = countChanged();
    if (!countChangedResult673)
      return std::unexpected(std::move(countChangedResult673.error()));
  }
  return {};
}

std::expected<void, StarException> MerchantPane::updateBuyTotal() {
  if (auto selected = m_itemGuiList->selectedWidget())
    {
      auto toUIntResult674 = selected->data().toUInt();
      if (!toUIntResult674)
        return std::unexpected(std::move(toUIntResult674.error()));
      m_buyTotal = std::move(*toUIntResult674) * m_buyCount;
    }
  else
    m_buyTotal = 0;

  auto setTextResult675 = m_buyTotalLabel->setText(toString(m_buyTotal));
  if (!setTextResult675)
    return std::unexpected(std::move(setTextResult675.error()));

  if (m_selectedIndex != NPos && m_buyCount > 0)
    m_buyButton->enable();
  else
    m_buyButton->disable();

  if (m_buyTotal > static_cast<int>(m_player->inventory()->currency("money"))) {
    m_buyTotalLabel->setColor(Color::Red);
    m_buyButton->disable();
  } else {
    m_buyTotalLabel->setColor(Color::White);
  }
  return {};
}

std::expected<void, StarException> MerchantPane::buy() {
  if (m_buyTotal > 0 && m_player->inventory()->consumeCurrency("money", m_buyTotal)) {
    auto countRemaining = m_buyCount;
    while (countRemaining > 0) {
      auto cloneResult676 = m_selectedItem->clone();
      if (!cloneResult676)
        return std::unexpected(std::move(cloneResult676.error()));
      auto buyItem = std::move(*cloneResult676);
      buyItem->setCount(m_selectedItem->count() * countRemaining);
      countRemaining -= buyItem->count();
      auto giveItemResult677 = m_player->giveItem(buyItem);
      if (!giveItemResult677)
        return std::unexpected(std::move(giveItemResult677.error()));
    }

    auto cloneResult678 = m_selectedItem->clone();
    if (!cloneResult678)
      return std::unexpected(std::move(cloneResult678.error()));
    auto reportItem = std::move(*cloneResult678);
    reportItem->setCount(reportItem->count() * m_buyCount, true);
    auto buySummary = JsonObject{{"item", reportItem->descriptor().toJson()}, {"total", m_buyTotal}};
    if (m_sourceEntityId != NullEntityId)
      {
        auto sendEntityMessageResult679 = m_worldClient->sendEntityMessage(m_sourceEntityId, "onBuy", {buySummary});
        if (!sendEntityMessageResult679)
          return std::unexpected(std::move(sendEntityMessageResult679.error()));
        
      }

    auto& guiContext = context();
    auto assetsResult680 = context().root().assets();
    if (!assetsResult680)
      return std::unexpected(std::move(assetsResult680.error()));
    auto jsonResult681 = std::move(*assetsResult680)->json("/merchant.config:buySound");
    if (!jsonResult681)
      return std::unexpected(std::move(jsonResult681.error()));
    auto toStringResult682 = std::move(*jsonResult681).toString();
    if (!toStringResult682)
      return std::unexpected(std::move(toStringResult682.error()));
    auto playAudioResult683 = guiContext.playAudio(std::move(*toStringResult682));
    if (!playAudioResult683)
      return std::unexpected(std::move(playAudioResult683.error()));

    auto buildItemListResult684 = buildItemList();
    if (!buildItemListResult684)
      return std::unexpected(std::move(buildItemListResult684.error()));

    auto updateBuyTotalResult685 = updateBuyTotal();
    if (!updateBuyTotalResult685)
      return std::unexpected(std::move(updateBuyTotalResult685.error()));
  }
  return {};
}

std::expected<void, StarException> MerchantPane::updateSellTotal() {
  m_sellTotal = 0;
  for (auto item : m_itemBag->items()) {
    if (item)
      m_sellTotal += std::round(item->price() * m_sellFactor);
  }
  auto setTextResult686 = m_sellTotalLabel->setText(toString(m_sellTotal));
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
    auto toJsonResult687 = m_itemBag->toJson();
    if (!toJsonResult687)
      return std::unexpected(std::move(toJsonResult687.error()));
    auto sellSummary = JsonObject{{"items", std::move(*toJsonResult687)}, {"total", m_sellTotal}};
    auto sendEntityMessageResult688 = m_worldClient->sendEntityMessage(m_sourceEntityId, "onSell", {sellSummary});
    if (!sendEntityMessageResult688)
      return std::unexpected(std::move(sendEntityMessageResult688.error()));

    auto addCurrencyResult689 = m_player->inventory()->addCurrency("money", m_sellTotal);
    if (!addCurrencyResult689)
      return std::unexpected(std::move(addCurrencyResult689.error()));
    m_itemBag->clearItems();
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
    auto toStringResult693 = std::move(*jsonResult692).toString();
    if (!toStringResult693)
      return std::unexpected(std::move(toStringResult693.error()));
    auto playAudioResult694 = guiContext.playAudio(std::move(*toStringResult693));
    if (!playAudioResult694)
      return std::unexpected(std::move(playAudioResult694.error()));
  }
  return {};
}

std::expected<int, StarException> MerchantPane::maxBuyCount() {
  if (auto selected = m_itemGuiList->selectedWidget()) {
    auto assetsResult695 = context().root().assets();
    if (!assetsResult695)
      return std::unexpected(std::move(assetsResult695.error()));
    auto assets = std::move(*assetsResult695);
    auto toUIntResult696 = selected->data().toUInt();
    if (!toUIntResult696)
      return std::unexpected(std::move(toUIntResult696.error()));
    auto unitPrice = std::move(*toUIntResult696);
    if (unitPrice == 0)
      return m_maxBuyCount;
    return std::min(m_maxBuyCount, static_cast<int>(std::floor(m_player->currency("money") / unitPrice)));
  } else {
    return 0;
  }
}

std::expected<void, StarException> MerchantPane::countChanged() {
  auto setTextResult697 = m_countTextBox->setText(strf("x{}", m_buyCount));
  if (!setTextResult697)
    return std::unexpected(std::move(setTextResult697.error()));
  auto updateBuyTotalResult698 = updateBuyTotal();
  if (!updateBuyTotalResult698)
    return std::unexpected(std::move(updateBuyTotalResult698.error()));

return {};
}

std::expected<void, StarException> MerchantPane::countTextChanged() {
  if (m_selectedIndex == NPos) {
    m_buyCount = 0;
    auto countChangedResult699 = countChanged();
    if (!countChangedResult699)
      return std::unexpected(std::move(countChangedResult699.error()));
  } else {
    auto replaceResult700 = m_countTextBox->getText().replace("x", "");
    if (!replaceResult700)
      return std::unexpected(std::move(replaceResult700.error()));
    auto countString = std::move(*replaceResult700);
    if (!countString.empty()) {
      auto parsed = lexicalCast<int>(countString);
      if (!parsed) {
        if (!parsed.error().is<BadLexicalCast>())
          return std::unexpected(std::move(parsed.error()));
        m_buyCount = 1;
      } else {
        auto maxBuyCountResult701 = maxBuyCount();
        if (!maxBuyCountResult701)
          return std::unexpected(std::move(maxBuyCountResult701.error()));
        m_buyCount = clamp<int>(*parsed, 1, std::move(*maxBuyCountResult701));
      }
      auto countChangedResult702 = countChanged();
      if (!countChangedResult702)
        return std::unexpected(std::move(countChangedResult702.error()));
    }
  }
  return {};
}

}
