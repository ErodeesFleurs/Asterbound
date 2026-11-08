import std;
import star.algorithm;
import star.json;
import star.json_extra;
import star.xx_hash;
import star.id_map;
import star.game_types;
import star.vector;
import star.casting;
import star.input_event;
import star.color;
import star.font;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.bi_map;
import star.string;
import star.text;
import star.asset_path;
import star.listener;
import star.set;
import star.audio;
import star.map;
import star.strong_typedef;
import star.rpc_promise;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.format;
import star.version;
import star.either;
import star.random;
import star.weighted_pool;
import star.ordered_set;
import star.byte_array;
import star.data_stream_devices;

typedef struct ZSTD_CCtx_s ZSTD_CCtx;
typedef struct ZSTD_DCtx_s ZSTD_DCtx;
typedef ZSTD_DCtx ZSTD_DStream;
typedef ZSTD_CCtx ZSTD_CStream;
import star.zstd_compression;
import star.net_compatibility;
import star.worker_pool;

import star.sector_array_2d;
import star.perlin;
import star.btree;
import star.lru_cache;
import star.btree_database;
import star.image;
import star.a_star;
import star.periodic_function;


import star.application;
import star.statistics_service;
import star.p2p_networking_service;
import star.user_generated_content_service;
import star.desktop_service;
import star.application_controller;
import star.renderer;
import star.lua_root;
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
import star.inventory_types;
import star.item_descriptor;

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.tile_damage;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.container_entity;


import star.container_interactor;

import star.game_timers;


import star.inventory;
import star.widget_parsing;
import star.gui_reader;
import star.host_address;
import star.sky_types;
import star.particle;
import star.weather_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.chat_types;
import star.ai_types;
import star.socket;
import star.tcp;
import star.liquid_types;
import star.tile_sector_array;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.celestial_types;
import star.tile_modification;
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
import star.net_packet_socket;
import star.universe_connection;
import star.entity_rendering_types;
import star.sky_render_data;
import star.parallax;
import star.cellular_light_array;
import star.cellular_lighting;
import star.world_render_data;
import star.world_structure;
import star.chat_action;
import star.entity_rendering;
import star.world;
import star.lua_components;
import star.world_client_state;
import star.interpolation_tracker;
import star.ambient;
import star.weather;
import star.world_client;
import star.world_client_thread;
import star.universe;
// TODO: make this more thread safe
import star.universe_client;
import star.item;
import star.progress_widget;
import star.animation;
import star.item_slot_widget;
import star.item_grid_widget;
import star.button_group;
import star.button_widget;
import star.animated_part_set;
import star.networked_animator;
import star.humanoid;
import star.physics_entity;
import star.anchorable_entity;
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.lounging_entities;
import star.chatty_entity;
import star.emote_entity;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.movement_controller;
import star.platformer_astar_types;
import star.actor_movement_controller;
import star.lua_actor_movement_component;
import star.lua_animation_component;
import star.radio_message_database;
import star.player;
import star.portrait_widget;
import star.pane_manager;
import star.label_widget;
import star.image_widget;
import star.interface_cursor;
import star.registered_pane_manager;
import star.main_interface_types;
import star.uuid;
import star.warping;
import star.main_interface;



import star.merchant_interface;
import star.augment_item;
import star.interaction_types;

import star.simple_tooltip;
import star.object_item;
import star.item_tooltip;
import star.player_tech;
import star.tech_database;

import star.player_inventory;
import star.player_companions;
import star.statistics;
import star.optional;

namespace Star {

InventoryPane::InventoryPane(GuiContext& guiContext, MainInterface& parent) : Pane(guiContext), m_parent(parent) {}

std::expected<std::shared_ptr<InventoryPane>, StarException> InventoryPane::create(GuiContext& guiContext, MainInterface& parent, std::shared_ptr<Player> player, std::shared_ptr<ContainerInteractor> containerInteractor) {
  auto pane = std::shared_ptr<InventoryPane>(new InventoryPane(guiContext, parent));
  auto base = pane->initializePane();
  if (!base)
    return std::unexpected(std::move(base.error()));
  auto initialized = pane->initialize(std::move(player), std::move(containerInteractor));
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return pane;
}

std::expected<void, StarException> InventoryPane::initialize(std::shared_ptr<Player> player, std::shared_ptr<ContainerInteractor> containerInteractor) {
  m_player = std::move(player);
  m_containerInteractor = std::move(containerInteractor);

  GuiReader invWindowReader(context());
  auto assetsResult1 = context().root().assets();
  if (!assetsResult1)
    return std::unexpected(std::move(assetsResult1.error()));
  auto jsonResult2 = std::move(*assetsResult1)->json("/interface/windowconfig/playerinventory.config");
  if (!jsonResult2)
    return std::unexpected(std::move(jsonResult2.error()));
  auto config = std::move(*jsonResult2);

  auto leftClickCallback = [this](String const& bagType, Widget& widget)  -> std::expected<void, StarException> {
    if (!m_parent)
      return std::unexpected(StarException("Inventory pane is detached from its interface"));
    auto& parent = m_parent->get();
    auto convertResult3 = rconvert<ItemGridWidget>(widget);
    if (!convertResult3)
      return std::unexpected(std::move(convertResult3.error()));
    auto& itemGrid = convertResult3->get();
    InventorySlot inventorySlot = BagSlot(bagType, itemGrid.selectedIndex());

    auto inventory = m_player->inventory();
    if (context().shiftHeld()) {
      {
        auto selectedItemResult4 = itemGrid.selectedItem();
        if (!selectedItemResult4)
          return std::unexpected(std::move(selectedItemResult4.error()));
        if (auto sourceItem = std::move(*selectedItemResult4)) {
          if (auto activeMerchantPane = parent.activeMerchantPane()) {
            auto takeSlotResult5 = inventory->takeSlot(inventorySlot);
            if (!takeSlotResult5)
              return std::unexpected(std::move(takeSlotResult5.error()));
            auto addItemsResult6 = activeMerchantPane->addItems(std::move(*takeSlotResult5));
            if (!addItemsResult6)
              return std::unexpected(std::move(addItemsResult6.error()));
            auto remainder = std::move(*addItemsResult6);
            if (remainder && !remainder->empty())
              {
                auto setItemResult7 = inventory->setItem(inventorySlot, remainder);
                if (!setItemResult7)
                  return std::unexpected(std::move(setItemResult7.error()));
                
              }
          } else if (m_containerInteractor->containerOpen()) {
            auto takeSlotResult8 = inventory->takeSlot(inventorySlot);
            if (!takeSlotResult8)
              return std::unexpected(std::move(takeSlotResult8.error()));
            auto addToContainerResult9 = m_containerInteractor->addToContainer(sourceItem);
            if (!addToContainerResult9)
              return std::unexpected(std::move(addToContainerResult9.error()));
            m_containerSource = inventorySlot;
            m_expectingSwap = true;
          } else {
            for (std::shared_ptr<Pane>& pane : parent.paneManager().getAllPanes()) {
              auto itemsAtResult10 = inventory->itemsAt(inventorySlot);
              if (!itemsAtResult10)
                return std::unexpected(std::move(itemsAtResult10.error()));
              auto shiftItemFromInventoryResult11 = pane->shiftItemFromInventory(std::move(*itemsAtResult10));
              if (!shiftItemFromInventoryResult11)
                return std::unexpected(std::move(shiftItemFromInventoryResult11.error()));
              auto remainder = std::move(*shiftItemFromInventoryResult11);
              if (remainder.has_value()) {
                auto setItemResult12 = inventory->setItem(inventorySlot, remainder.value_or(std::shared_ptr<Item>{}));
                if (!setItemResult12)
                  return std::unexpected(std::move(setItemResult12.error()));
                break;
              }
            }
          }
        }
      }
    } else {
      auto shiftSwapResult13 = inventory->shiftSwap(inventorySlot);
      if (!shiftSwapResult13)
        return std::unexpected(std::move(shiftSwapResult13.error()));
    }
  
  return {};
};

  auto rightClickCallback = [this](InventorySlot slot)  -> std::expected<void, StarException> {
    auto inventory = m_player->inventory();
    {
      auto itemsAtResult14 = inventory->itemsAt(slot);
      if (!itemsAtResult14)
        return std::unexpected(std::move(itemsAtResult14.error()));
      if (std::shared_ptr<Item> slotItem = std::move(*itemsAtResult14)) {
        auto swapItem = inventory->swapSlotItem();
        if (!swapItem || swapItem->empty() || swapItem->couldStack(optionalRef(slotItem))) {
          std::uint64_t count = swapItem ? swapItem->couldStack(optionalRef(slotItem)) : slotItem->maxStack();
          if (context().shiftHeld())
            count = std::max<std::uint64_t>(1, std::min<std::uint64_t>(count, slotItem->count() / 2));
          else
            count = 1;
  
          {
            auto takeResult15 = slotItem->take(count);
            if (!takeResult15)
              return std::unexpected(std::move(takeResult15.error()));
            if (auto taken = std::move(*takeResult15)) {
              if (swapItem)
                swapItem->stackWith(optionalRef(taken));
              else
                {
                  auto setSwapSlotItemResult16 = inventory->setSwapSlotItem(taken);
                  if (!setSwapSlotItemResult16)
                    return std::unexpected(std::move(setSwapSlotItemResult16.error()));
                  
                }
            }
          }
        } else if (auto augment = as<AugmentItem>(swapItem)) {
          {
            auto applyToResult17 = augment->applyTo(slotItem);
            if (!applyToResult17)
              return std::unexpected(std::move(applyToResult17.error()));
            if (auto augmented = std::move(*applyToResult17))
              {
                auto setItemResult18 = inventory->setItem(slot, augmented);
                if (!setItemResult18)
                  return std::unexpected(std::move(setItemResult18.error()));
                
              }
          }
        }
      }
      else if (auto swapSlot = inventory->swapSlotItem()) {
        if (std::holds_alternative<EquipmentSlot>(slot)) {
          if (inventory->itemAllowedAsEquipment(swapSlot, std::get<EquipmentSlot>(slot)))
            {
              auto takeResult19 = swapSlot->take(1);
              if (!takeResult19)
                return std::unexpected(std::move(takeResult19.error()));
              auto setItemResult20 = inventory->setItem(slot, std::move(*takeResult19));
              if (!setItemResult20)
                return std::unexpected(std::move(setItemResult20.error()));
              
            }
        } else if (std::holds_alternative<TrashSlot>(slot)) {
          auto takeResult21 = swapSlot->take(1);
          if (!takeResult21)
            return std::unexpected(std::move(takeResult21.error()));
          auto setItemResult22 = inventory->setItem(slot, std::move(*takeResult21));
          if (!setItemResult22)
            return std::unexpected(std::move(setItemResult22.error()));
        } else if (std::holds_alternative<BagSlot>(slot)) {
          {
            auto itemAllowedInBagResult23 = inventory->itemAllowedInBag(swapSlot, std::get<BagSlot>(slot).first);
            if (!itemAllowedInBagResult23)
              return std::unexpected(std::move(itemAllowedInBagResult23.error()));
            if (std::move(*itemAllowedInBagResult23))
              {
                auto takeResult24 = swapSlot->take(1);
                if (!takeResult24)
                  return std::unexpected(std::move(takeResult24.error()));
                auto setItemResult25 = inventory->setItem(slot, std::move(*takeResult24));
                if (!setItemResult25)
                  return std::unexpected(std::move(setItemResult25.error()));
                
              }
          }
        }
      }
    }
  
  return {};
};

  auto bagGridCallback = [rightClickCallback](String const& bagType, Widget& widget)  -> std::expected<void, StarException> {
    auto convertResult26 = rconvert<ItemGridWidget>(widget);
    if (!convertResult26)
      return std::unexpected(std::move(convertResult26.error()));
    auto slot = BagSlot(bagType, convertResult26->get().selectedIndex());
    auto rightClickCallbackResult27 = rightClickCallback(slot);
    if (!rightClickCallbackResult27)
      return std::unexpected(std::move(rightClickCallbackResult27.error()));
  
  return {};
};

  auto middleClickCallback = [this](String const& bagType, Widget& widget)  -> std::expected<void, StarException> {
    if (!m_player->inWorld()) 
      return {};

    auto convertResult28 = rconvert<ItemGridWidget>(widget);
    if (!convertResult28)
      return std::unexpected(std::move(convertResult28.error()));
    auto& itemGrid = convertResult28->get();
    InventorySlot inventorySlot = BagSlot(bagType, itemGrid.selectedIndex());
    auto inventory = m_player->inventory();
    {
      auto selectedItemResult29 = itemGrid.selectedItem();
      if (!selectedItemResult29)
        return std::unexpected(std::move(selectedItemResult29.error()));
      if (auto sourceItem = as<ObjectItem>(std::move(*selectedItemResult29))) {
        {
          auto instanceValueResult30 = sourceItem->instanceValue("interactAction");
          if (!instanceValueResult30)
            return std::unexpected(std::move(instanceValueResult30.error()));
          if (auto actionTypeName = std::move(*instanceValueResult30)) {
            auto const& action_names = InteractActionTypeNames;
            if (!action_names)
              return std::unexpected(action_names.error());
            auto action_name = actionTypeName.toString();
            if (!action_name)
              return std::unexpected(std::move(action_name.error()));
            auto action_lookup = (*action_names).get_left(std::move(*action_name));
            if (!action_lookup)
              return std::unexpected(std::move(action_lookup.error()));
            auto actionType = action_lookup->get();
            if (actionType >= InteractActionType::OpenCraftingInterface && actionType <= InteractActionType::ScriptPane) {
              auto instanceValueResult34 = sourceItem->instanceValue("interactData", Json());
              if (!instanceValueResult34)
                return std::unexpected(std::move(instanceValueResult34.error()));
              auto actionData = std::move(*instanceValueResult34);
              if (actionData.isType(Json::Type::Object))
                {
                  auto setResult35 = actionData.set("openWithInventory", false);
                  if (!setResult35)
                    return std::unexpected(std::move(setResult35.error()));
                  actionData = std::move(*setResult35);
                }
              InteractAction action(actionType, NullEntityId, actionData);
              auto interactResult36 = m_player->interact(action);
              if (!interactResult36)
                return std::unexpected(std::move(interactResult36.error()));
            }
          }
        }
      }
    }
  
  return {};
};

  auto getResult37 = config.get("bagConfig");
  if (!getResult37)
    return std::unexpected(std::move(getResult37.error()));
  Json itemBagConfig = std::move(*getResult37);
  auto toObjectResult38 = itemBagConfig.toObject();
  if (!toObjectResult38)
    return std::unexpected(std::move(toObjectResult38.error()));
  auto bagOrder = std::move(*toObjectResult38).keys();
  auto sortByComputedValueResult41 = sortByComputedValue(bagOrder, [&itemBagConfig](String const& name) -> std::expected<std::int64_t, StarException> {
      auto getResult39 = itemBagConfig.get(name);
      if (!getResult39)
        return std::unexpected(std::move(getResult39.error()));
      auto getIntResult40 = std::move(*getResult39).getInt("order", 0);
      if (!getIntResult40)
        return std::unexpected(std::move(getIntResult40.error()));
      return std::move(*getIntResult40);
    });
  if (!sortByComputedValueResult41)
    return std::unexpected(std::move(sortByComputedValueResult41.error()));
  for (auto const& name : bagOrder) {
    auto getResult42 = itemBagConfig.get(name);
    if (!getResult42)
      return std::unexpected(std::move(getResult42.error()));
    auto getStringResult43 = std::move(*getResult42).getString("itemGrid");
    if (!getStringResult43)
      return std::unexpected(std::move(getStringResult43.error()));
    auto itemGrid = std::move(*getStringResult43);
    invWindowReader.registerCallback(
      itemGrid, [leftClickCallback, name](Widget& widget) mutable -> std::expected<void, StarException> {
        return leftClickCallback(name, widget);
      });
    invWindowReader.registerCallback(
      itemGrid + ".right", [bagGridCallback, name](Widget& widget) mutable -> std::expected<void, StarException> {
        return bagGridCallback(name, widget);
      });
    invWindowReader.registerCallback(
      itemGrid + ".middle", [middleClickCallback, name](Widget& widget) mutable -> std::expected<void, StarException> {
        return middleClickCallback(name, widget);
      });
  }

  invWindowReader.registerCallback("close", [this](Widget&)  -> std::expected<void, StarException> {
      dismiss();
    
  return {};
});

  invWindowReader.registerCallback("sort", [this](Widget&)  -> std::expected<void, StarException> {
      auto condenseBagStacksResult44 = m_player->inventory()->condenseBagStacks(m_selectedTab);
      if (!condenseBagStacksResult44)
        return std::unexpected(std::move(condenseBagStacksResult44.error()));
      auto sortBagResult45 = m_player->inventory()->sortBag(m_selectedTab);
      if (!sortBagResult45)
        return std::unexpected(std::move(sortBagResult45.error()));
      // Don't show sorted items as new items
      auto updateItemStateResult46 = m_itemGrids[m_selectedTab]->updateItemState();
      if (!updateItemStateResult46)
        return std::unexpected(std::move(updateItemStateResult46.error()));
      m_itemGrids[m_selectedTab]->clearChangedSlots();
    
  return {};
});

  invWindowReader.registerCallback("gridModeSelector", [this](Widget& widget)  -> std::expected<void, StarException> {
      auto convertResult47 = rconvert<ButtonWidget>(widget);
      if (!convertResult47)
        return std::unexpected(std::move(convertResult47.error()));
      auto toStringResult48 = convertResult47->get().data().toString();
      if (!toStringResult48)
        return std::unexpected(std::move(toStringResult48.error()));
      auto selected = std::move(*toStringResult48);
      auto tab_lookup = m_tabButtonData.key_of(selected);
      if (!tab_lookup)
        return std::unexpected(std::move(tab_lookup.error()));
      auto tab_selected = selectTab(std::move(*tab_lookup));
      if (!tab_selected)
        return std::unexpected(std::move(tab_selected.error()));

      return {};
});

  auto registerSlotCallbacks = [&](String name, InventorySlot slot)  -> std::expected<void, StarException> {
    invWindowReader.registerCallback(name, [this, slot](Widget& paneObj)  -> std::expected<void, StarException> {
        if (asRef<ItemSlotWidget>(paneObj))
          {
            auto shiftSwapResult51 = m_player->inventory()->shiftSwap(slot);
            if (!shiftSwapResult51)
              return std::unexpected(std::move(shiftSwapResult51.error()));
            
          }
        else
          return std::unexpected(GuiException("Invalid object type, expected ItemSlotWidget"));
      
  return {};
});
    invWindowReader.registerCallback(name + ".right", [slot, rightClickCallback](Widget& paneObj)  -> std::expected<void, StarException> {
        if (asRef<ItemSlotWidget>(paneObj))
          {
            auto rightClickCallbackResult52 = rightClickCallback(slot);
            if (!rightClickCallbackResult52)
              return std::unexpected(std::move(rightClickCallbackResult52.error()));
            
          }
        else
          return std::unexpected(GuiException("Invalid object type, expected ItemSlotWidget"));
      
  return {};
});
  
  return {};
};

  {
    auto const& enumResult53 = EquipmentSlotNames;
    if (!enumResult53)
      return std::unexpected(enumResult53.error());
    for (auto const p : (*enumResult53)) {
      EquipmentSlot slot = p.first;
      auto registerSlotCallbacksResult54 = registerSlotCallbacks(p.second, slot);
      if (!registerSlotCallbacksResult54)
        return std::unexpected(std::move(registerSlotCallbacksResult54.error()));
      invWindowReader.registerCallback(p.second + ".middle", [slot, this](Widget&)  -> std::expected<void, StarException> {
        auto inventory = m_player->inventory();
        inventory->setEquipmentVisibility(slot, !inventory->equipmentVisibility(slot));
      
    return {};
  });
    }
  }
  auto registerSlotCallbacksResult55 = registerSlotCallbacks("trash", TrashSlot());
  if (!registerSlotCallbacksResult55)
    return std::unexpected(std::move(registerSlotCallbacksResult55.error()));

  auto getResult56 = config.get("paneLayout");
  if (!getResult56)
    return std::unexpected(std::move(getResult56.error()));
  auto constructResult57 = invWindowReader.construct(std::move(*getResult56), *this);
  if (!constructResult57)
    return std::unexpected(std::move(constructResult57.error()));

  auto fetchChildResult58 = fetchChild<ItemSlotWidget>("trash");
  if (!fetchChildResult58)
    return std::unexpected(std::move(fetchChildResult58.error()));
  m_trashSlot = std::move(*fetchChildResult58);
  auto getResult59 = config.get("trashBurnTimeout");
  if (!getResult59)
    return std::unexpected(std::move(getResult59.error()));
  auto toFloatResult60 = std::move(*getResult59).toFloat();
  if (!toFloatResult60)
    return std::unexpected(std::move(toFloatResult60.error()));
  m_trashBurn = GameTimer(std::move(*toFloatResult60));

  auto head_overlay = fetchChild<ImageWidget>("techHeadDisabled");
  if (!head_overlay)
    return std::unexpected(std::move(head_overlay.error()));
  m_disabledTechOverlays.push_back(std::move(*head_overlay));
  auto body_overlay = fetchChild<ImageWidget>("techBodyDisabled");
  if (!body_overlay)
    return std::unexpected(std::move(body_overlay.error()));
  m_disabledTechOverlays.push_back(std::move(*body_overlay));
  auto legs_overlay = fetchChild<ImageWidget>("techLegsDisabled");
  if (!legs_overlay)
    return std::unexpected(std::move(legs_overlay.error()));
  m_disabledTechOverlays.push_back(std::move(*legs_overlay));

  {
    auto const& enumResult64 = EquipmentSlotNames;
    if (!enumResult64)
      return std::unexpected(enumResult64.error());
    for (auto const p : (*enumResult64)) {
      {
        auto fetchChildResult65 = fetchChild<ItemSlotWidget>(p.second);
        if (!fetchChildResult65)
          return std::unexpected(std::move(fetchChildResult65.error()));
        if (auto itemSlot = std::move(*fetchChildResult65)) {
          auto itemsAtResult66 = m_player->inventory()->itemsAt(p.first);
          if (!itemsAtResult66)
            return std::unexpected(std::move(itemsAtResult66.error()));
          itemSlot->setItem(std::move(*itemsAtResult66));
          {
            auto findChildResult67 = itemSlot->findChild<ImageWidget>("hidden");
            if (!findChildResult67)
              return std::unexpected(std::move(findChildResult67.error()));
            if (auto indicator = std::move(*findChildResult67))
              {
                auto setVisibilityResult68 = indicator->setVisibility(!m_player->inventory()->equipmentVisibility(p.first));
                if (!setVisibilityResult68)
                  return std::unexpected(std::move(setVisibilityResult68.error()));
                
              }
          }
          if (p.first >= EquipmentSlot::Cosmetic1)
            {
              auto hideResult69 = itemSlot->hide();
              if (!hideResult69)
                return std::unexpected(std::move(hideResult69.error()));
              
            }
        }
      }
    }
  }

  for (auto const& name : bagOrder) {
    auto getResult70 = itemBagConfig.get(name);
    if (!getResult70)
      return std::unexpected(std::move(getResult70.error()));
    auto itemTab = std::move(*getResult70);
    auto getStringResult71 = itemTab.getString("itemGrid");
    if (!getStringResult71)
      return std::unexpected(std::move(getStringResult71.error()));
    auto fetchChildResult72 = fetchChild<ItemGridWidget>(std::move(*getStringResult71));
    if (!fetchChildResult72)
      return std::unexpected(std::move(fetchChildResult72.error()));
    m_itemGrids[name] = std::move(*fetchChildResult72);
    auto setItemBagResult73 = m_itemGrids[name]->setItemBag(m_player->inventory()->bagContents(name));
    if (!setItemBagResult73)
      return std::unexpected(std::move(setItemBagResult73.error()));
    auto hideResult74 = m_itemGrids[name]->hide();
    if (!hideResult74)
      return std::unexpected(std::move(hideResult74.error()));
    auto getStringResult75 = itemTab.getString("newItemMarker");
    if (!getStringResult75)
      return std::unexpected(std::move(getStringResult75.error()));
    auto fetchChildResult76 = fetchChild<Widget>(std::move(*getStringResult75));
    if (!fetchChildResult76)
      return std::unexpected(std::move(fetchChildResult76.error()));
    m_newItemMarkers[name] = std::move(*fetchChildResult76);
    auto getStringResult77 = itemTab.getString("tabButtonData");
    if (!getStringResult77)
      return std::unexpected(std::move(getStringResult77.error()));
    m_tabButtonData[name] = std::move(*getStringResult77);
  }
  auto selectTabResult78 = selectTab(bagOrder[0]);
  if (!selectTabResult78)
    return std::unexpected(std::move(selectTabResult78.error()));

  auto fetchChildResult79 = fetchChild<PortraitWidget>("portrait");
  if (!fetchChildResult79)
    return std::unexpected(std::move(fetchChildResult79.error()));
  auto centralPortrait = std::move(*fetchChildResult79);
  auto setEntityResult80 = centralPortrait->setEntity(m_player);
  if (!setEntityResult80)
    return std::unexpected(std::move(setEntityResult80.error()));

  auto createResult81 = PortraitWidget::create(context(), m_player, PortraitMode::Bust);
  if (!createResult81)
    return std::unexpected(std::move(createResult81.error()));
  auto portrait = std::move(*createResult81);
  auto setIconModeResult82 = portrait->setIconMode();
  if (!setIconModeResult82)
    return std::unexpected(std::move(setIconModeResult82.error()));
  auto nameResult83 = m_player->name();
  if (!nameResult83)
    return std::unexpected(std::move(nameResult83.error()));
  auto getStringResult84 = config.getString("subtitle");
  if (!getStringResult84)
    return std::unexpected(std::move(getStringResult84.error()));
  auto setTitleResult85 = setTitle(portrait, std::move(*nameResult83), std::move(*getStringResult84));
  if (!setTitleResult85)
    return std::unexpected(std::move(setTitleResult85.error()));

  {
    auto getBoolResult86 = config.getBool("alwaysDisplayCosmetics", false);
    if (!getBoolResult86)
      return std::unexpected(std::move(getBoolResult86.error()));
    if ((m_displayingCosmetics = m_alwaysDisplayCosmetics = std::move(*getBoolResult86))) {
      {
        auto const& enumResult87 = EquipmentSlotNames;
        if (!enumResult87)
          return std::unexpected(enumResult87.error());
        for (auto const& p : (*enumResult87)) {
          if (p.first >= EquipmentSlot::Cosmetic1) {
            {
              auto fetchChildResult88 = fetchChild<ItemSlotWidget>(p.second);
              if (!fetchChildResult88)
                return std::unexpected(std::move(fetchChildResult88.error()));
              if (auto itemSlot = std::move(*fetchChildResult88))
                {
                  auto setVisibilityResult89 = itemSlot->setVisibility(true);
                  if (!setVisibilityResult89)
                    return std::unexpected(std::move(setVisibilityResult89.error()));
                  
                }
            }
          }
        }
      }
    }
  }

  m_expectingSwap = false;

  if (auto item = m_player->inventory()->swapSlotItem())
    m_currentSwapSlotItem = item->descriptor();
  auto getResult90 = config.get("sounds");
  if (!getResult90)
    return std::unexpected(std::move(getResult90.error()));
  auto getResult91 = std::move(*getResult90).get("pickup");
  if (!getResult91)
    return std::unexpected(std::move(getResult91.error()));
  auto jsonToStringListResult92 = jsonToStringList(std::move(*getResult91));
  if (!jsonToStringListResult92)
    return std::unexpected(std::move(jsonToStringListResult92.error()));
  m_pickUpSounds = std::move(*jsonToStringListResult92);
  auto getResult93 = config.get("sounds");
  if (!getResult93)
    return std::unexpected(std::move(getResult93.error()));
  auto getResult94 = std::move(*getResult93).get("putdown");
  if (!getResult94)
    return std::unexpected(std::move(getResult94.error()));
  auto jsonToStringListResult95 = jsonToStringList(std::move(*getResult94));
  if (!jsonToStringListResult95)
    return std::unexpected(std::move(jsonToStringListResult95.error()));
  m_putDownSounds = std::move(*jsonToStringListResult95);
  auto getResult96 = config.get("sounds");
  if (!getResult96)
    return std::unexpected(std::move(getResult96.error()));
  auto getResult97 = std::move(*getResult96).get("someup");
  if (!getResult97)
    return std::unexpected(std::move(getResult97.error()));
  auto jsonToStringListResult98 = jsonToStringList(std::move(*getResult97));
  if (!jsonToStringListResult98)
    return std::unexpected(std::move(jsonToStringListResult98.error()));
  m_someUpSounds = std::move(*jsonToStringListResult98);
  auto getResult99 = config.get("sounds");
  if (!getResult99)
    return std::unexpected(std::move(getResult99.error()));
  auto getResult100 = std::move(*getResult99).get("somedown");
  if (!getResult100)
    return std::unexpected(std::move(getResult100.error()));
  auto jsonToStringListResult101 = jsonToStringList(std::move(*getResult100));
  if (!jsonToStringListResult101)
    return std::unexpected(std::move(jsonToStringListResult101.error()));
  m_someDownSounds = std::move(*jsonToStringListResult101);

return {};
}

std::expected<void, StarException> InventoryPane::displayed() {
  auto displayedResult102 = Pane::displayed();
  if (!displayedResult102)
    return std::unexpected(std::move(displayedResult102.error()));
  m_expectingSwap = false;

  for (auto grid : m_itemGrids)
    {
      auto updateItemStateResult103 = grid.second->updateItemState();
      if (!updateItemStateResult103)
        return std::unexpected(std::move(updateItemStateResult103.error()));
      
    }
  m_itemGrids[m_selectedTab]->indicateChangedSlots();
  return {};
}

std::expected<std::shared_ptr<Pane>, StarException> InventoryPane::createTooltip(Vec2I const& screenPosition) {
  std::shared_ptr<Item> item;
  if (auto child = getChildAt(screenPosition)) {
    if (auto itemSlot = as<ItemSlotWidget>(child)) {
      item = itemSlot->item();
      if (!item) {
        auto widgetData = itemSlot->data();
        if (widgetData && widgetData.type() == Json::Type::Object) {
          {
            auto optStringResult104 = widgetData.optString("tooltipText");
            if (!optStringResult104)
              return std::unexpected(std::move(optStringResult104.error()));
            if (auto text = std::move(*optStringResult104))
              {
                auto buildTooltipResult105 = SimpleTooltipBuilder::buildTooltip(context(), *text);
                if (!buildTooltipResult105)
                  return std::unexpected(std::move(buildTooltipResult105.error()));
                return std::move(*buildTooltipResult105);
              }
          }
        }
      }
    }
    if (auto itemGrid = as<ItemGridWidget>(child))
      {
        auto itemAtResult106 = itemGrid->itemAt(screenPosition);
        if (!itemAtResult106)
          return std::unexpected(std::move(itemAtResult106.error()));
        item = std::move(*itemAtResult106);
      }
  }
  if (item)
    {
      auto buildItemTooltipResult107 = ItemTooltipBuilder::buildItemTooltip(context(), item, m_player);
      if (!buildItemTooltipResult107)
        return std::unexpected(std::move(buildItemTooltipResult107.error()));
      return std::move(*buildItemTooltipResult107);
    }

  auto techDatabaseResult108 = context().root().techDatabase();
  if (!techDatabaseResult108)
    return std::unexpected(std::move(techDatabaseResult108.error()));
  auto techDatabase = std::move(*techDatabaseResult108);
  {
    auto const& enumResult109 = TechTypeNames;
    if (!enumResult109)
      return std::unexpected(enumResult109.error());
    for (auto const& p : (*enumResult109)) {
      {
        auto fetchChildResult110 = fetchChild<ImageWidget>(Star::format("tech{}", p.second));
        if (!fetchChildResult110)
          return std::unexpected(std::move(fetchChildResult110.error()));
        if (auto techIcon = std::move(*fetchChildResult110)) {
          if (techIcon->screenBoundRect().contains(screenPosition)) {
            if (auto techModule = m_player->techs()->equippedTechs().maybe(p.first))
              if (techDatabase->contains(*techModule))
                {
                  auto techResult111 = techDatabase->tech(*techModule);
                  if (!techResult111)
                    return std::unexpected(std::move(techResult111.error()));
                  auto buildTooltipResult112 = SimpleTooltipBuilder::buildTooltip(context(), std::move(*techResult111).description);
                  if (!buildTooltipResult112)
                    return std::unexpected(std::move(buildTooltipResult112.error()));
                  return std::move(*buildTooltipResult112);
                }
          }
        }
      }
    }
  }

  return {};
}

std::expected<bool, StarException> InventoryPane::sendEvent(InputEvent const& event) {
  if (m_alwaysDisplayCosmetics)
    {
      auto sendEventResult113 = Pane::sendEvent(event);
      if (!sendEventResult113)
        return std::unexpected(std::move(sendEventResult113.error()));
      return std::move(*sendEventResult113);
    }

  if (auto mousePosition = Widget::context().mousePosition(event)) {
    bool displayingCosmetics = false;
    {
      auto const& enumResult114 = EquipmentSlotNames;
      if (!enumResult114)
        return std::unexpected(enumResult114.error());
      for (auto const& p : (*enumResult114)) {
        if (p.first >= EquipmentSlot::HeadCosmetic) {
          {
            auto fetchChildResult115 = fetchChild<ItemSlotWidget>(p.second);
            if (!fetchChildResult115)
              return std::unexpected(std::move(fetchChildResult115.error()));
            if (auto itemSlot = std::move(*fetchChildResult115)) {
              if ((displayingCosmetics = itemSlot->inMember(*mousePosition)))
                break;
            }
          }
        }
      }
    }

    if (m_displayingCosmetics != displayingCosmetics) {
      {
        auto const& enumResult116 = EquipmentSlotNames;
        if (!enumResult116)
          return std::unexpected(enumResult116.error());
        for (auto const& p : (*enumResult116)) {
          if (p.first >= EquipmentSlot::Cosmetic1) {
            {
              auto fetchChildResult117 = fetchChild<ItemSlotWidget>(p.second);
              if (!fetchChildResult117)
                return std::unexpected(std::move(fetchChildResult117.error()));
              if (auto itemSlot = std::move(*fetchChildResult117))
                {
                  auto setVisibilityResult118 = itemSlot->setVisibility(displayingCosmetics);
                  if (!setVisibilityResult118)
                    return std::unexpected(std::move(setVisibilityResult118.error()));
                  
                }
            }
          }
        }
      }
      m_displayingCosmetics = displayingCosmetics;
    }
  }

  auto sendEventResult119 = Pane::sendEvent(event);
  if (!sendEventResult119)
    return std::unexpected(std::move(sendEventResult119.error()));
  return std::move(*sendEventResult119);
}

std::expected<bool, StarException> InventoryPane::giveContainerResult(ContainerResult result) {
  if (!m_expectingSwap)
    return false;

  for (auto& item : result) {
    auto inv = m_player->inventory();
    auto triggerPickupEventsResult120 = m_player->triggerPickupEvents(item);
    if (!triggerPickupEventsResult120)
      return std::unexpected(std::move(triggerPickupEventsResult120.error()));

    auto stackWithResult121 = inv->stackWith(m_containerSource, item);
    if (!stackWithResult121)
      return std::unexpected(std::move(stackWithResult121.error()));
    auto remainder = std::move(*stackWithResult121);
    if (remainder && !remainder->empty())
      {
        auto giveItemResult122 = m_player->giveItem(remainder);
        if (!giveItemResult122)
          return std::unexpected(std::move(giveItemResult122.error()));
        
      }
  }

  m_expectingSwap = false;
  return true;
}

std::expected<void, StarException> InventoryPane::updateItems() {
  for (auto& p : m_itemGrids)
    {
      auto updateItemStateResult123 = p.second->updateItemState();
      if (!updateItemStateResult123)
        return std::unexpected(std::move(updateItemStateResult123.error()));
      
    }

return {};
}

bool InventoryPane::containsNewItems() const {
  for (auto& p : m_itemGrids) {
    if (p.second->slotsChanged())
      return true;
  }
  return false;
}

std::expected<void, StarException> InventoryPane::clearChangedSlots() {
  for (auto& p : m_itemGrids) {
    auto updateItemStateResult124 = p.second->updateItemState();
    if (!updateItemStateResult124)
      return std::unexpected(std::move(updateItemStateResult124.error()));
    p.second->clearChangedSlots();
  }
  return {};
}

std::expected<void, StarException> InventoryPane::update(float dt) {
  auto inventory = m_player->inventory();
  auto& context = Widget::context();

  HashSet<std::shared_ptr<Item>> customBarItems;
  for (std::uint8_t i = 0; i < inventory->customBarIndexes(); ++i) {
    {
      auto customBarPrimarySlotResult125 = inventory->customBarPrimarySlot(i);
      if (!customBarPrimarySlotResult125)
        return std::unexpected(std::move(customBarPrimarySlotResult125.error()));
      if (auto primarySlot = std::move(*customBarPrimarySlotResult125)) {
        {
          auto itemsAtResult126 = inventory->itemsAt(*primarySlot);
          if (!itemsAtResult126)
            return std::unexpected(std::move(itemsAtResult126.error()));
          if (auto primaryItem = std::move(*itemsAtResult126))
            customBarItems.add(primaryItem);
        }
      }
    }
    {
      auto customBarSecondarySlotResult127 = inventory->customBarSecondarySlot(i);
      if (!customBarSecondarySlotResult127)
        return std::unexpected(std::move(customBarSecondarySlotResult127.error()));
      if (auto secondarySlot = std::move(*customBarSecondarySlotResult127)) {
        {
          auto itemsAtResult128 = inventory->itemsAt(*secondarySlot);
          if (!itemsAtResult128)
            return std::unexpected(std::move(itemsAtResult128.error()));
          if (auto secondaryItem = std::move(*itemsAtResult128))
            customBarItems.add(secondaryItem);
        }
      }
    }
  }

  auto itemsAtResult129 = inventory->itemsAt(TrashSlot());
  if (!itemsAtResult129)
    return std::unexpected(std::move(itemsAtResult129.error()));
  m_trashSlot->setItem(std::move(*itemsAtResult129));
  m_trashSlot->showLinkIndicator(customBarItems.contains(m_trashSlot->item()));
  if (auto trashItem = m_trashSlot->item()) {
    if (m_trashBurn.tick(dt) && trashItem->count() > 0) {
      m_player->statistics()->recordEvent("trashItem", JsonObject{
          {"itemName", trashItem->name()},
          {"count", trashItem->count()},
          {"category", trashItem->category()}
        });
      auto takeResult130 = trashItem->take(trashItem->count());
      if (!takeResult130)
        return std::unexpected(std::move(takeResult130.error()));
    }
  } else {
    m_trashBurn.reset();
  }
  m_trashSlot->setProgress(m_trashBurn.timer / m_trashBurn.time);

  {
    auto const& enumResult131 = EquipmentSlotNames;
    if (!enumResult131)
      return std::unexpected(enumResult131.error());
    for (auto const& p : (*enumResult131)) {
      {
        auto fetchChildResult132 = fetchChild<ItemSlotWidget>(p.second);
        if (!fetchChildResult132)
          return std::unexpected(std::move(fetchChildResult132.error()));
        if (auto itemSlot = std::move(*fetchChildResult132)) {
          auto itemsAtResult133 = inventory->itemsAt(p.first);
          if (!itemsAtResult133)
            return std::unexpected(std::move(itemsAtResult133.error()));
          itemSlot->setItem(std::move(*itemsAtResult133));
          itemSlot->showLinkIndicator(customBarItems.contains(itemSlot->item()));
          {
            auto findChildResult134 = itemSlot->findChild<ImageWidget>("hidden");
            if (!findChildResult134)
              return std::unexpected(std::move(findChildResult134.error()));
            if (auto indicator = std::move(*findChildResult134))
              {
                auto setVisibilityResult135 = indicator->setVisibility(!inventory->equipmentVisibility(p.first));
                if (!setVisibilityResult135)
                  return std::unexpected(std::move(setVisibilityResult135.error()));
                
              }
          }
        }
      }
    }
  }

  auto techDatabaseResult136 = context.root().techDatabase();
  if (!techDatabaseResult136)
    return std::unexpected(std::move(techDatabaseResult136.error()));
  auto techDatabase = std::move(*techDatabaseResult136);
  {
    auto const& enumResult137 = TechTypeNames;
    if (!enumResult137)
      return std::unexpected(enumResult137.error());
    for (auto const& p : (*enumResult137)) {
      {
        auto fetchChildResult138 = fetchChild<ImageWidget>(Star::format("tech{}", p.second));
        if (!fetchChildResult138)
          return std::unexpected(std::move(fetchChildResult138.error()));
        if (auto techIcon = std::move(*fetchChildResult138)) {
          if (auto techModule = m_player->techs()->equippedTechs().maybe(p.first)) {
            if (techDatabase->contains(*techModule)) {
              auto techResult139 = techDatabase->tech(*techModule);
              if (!techResult139)
                return std::unexpected(std::move(techResult139.error()));
              auto setImageResult140 = techIcon->setImage(std::move(*techResult139).icon);
              if (!setImageResult140)
                return std::unexpected(std::move(setImageResult140.error()));
              continue;
            }
          }
          auto setImageResult141 = techIcon->setImage("");
          if (!setImageResult141)
            return std::unexpected(std::move(setImageResult141.error()));
        }
      }
    }
  }

  if (std::shared_ptr<Item> swapSlot = inventory->swapSlotItem()) {
    {
      auto itemAllowedInBagResult142 = inventory->itemAllowedInBag(swapSlot, m_selectedTab);
      if (!itemAllowedInBagResult142)
        return std::unexpected(std::move(itemAllowedInBagResult142.error()));
      if (!std::move(*itemAllowedInBagResult142)) {
        for (auto& pair : m_itemGrids) {
          {
            bool condition144 = static_cast<bool>(pair.first != m_selectedTab);
            if (condition144) {
              auto itemAllowedInBagResult143 = inventory->itemAllowedInBag(swapSlot, pair.first);
              if (!itemAllowedInBagResult143)
                return std::unexpected(std::move(itemAllowedInBagResult143.error()));
              condition144 = static_cast<bool>(std::move(*itemAllowedInBagResult143));
            }
            if (condition144) {
              auto selectTabResult145 = selectTab(pair.first);
              if (!selectTabResult145)
                return std::unexpected(std::move(selectTabResult145.error()));
              break;
            }
          }
        }
      }
    }
  }

  for (auto p : m_itemGrids) {
    auto updateItemStateResult146 = p.second->updateItemState();
    if (!updateItemStateResult146)
      return std::unexpected(std::move(updateItemStateResult146.error()));
    for (std::size_t i = 0; i < p.second->itemSlots(); ++i) {
      if (auto itemWidget = p.second->itemWidgetAt(i))
        itemWidget->showLinkIndicator(customBarItems.contains(itemWidget->item()));
      else
        {
          auto warnResult147 = context.root().logger().warn("Could not find item widget {} in item grid {}!", i, p.first);
          if (!warnResult147)
            return std::unexpected(std::move(warnResult147.error()));
          
        }
    }
  }

  m_itemGrids[m_selectedTab]->clearChangedSlots();

  for (auto& pair : m_newItemMarkers) {
    if (m_itemGrids[pair.first]->slotsChanged())
      {
        auto showResult148 = pair.second->show();
        if (!showResult148)
          return std::unexpected(std::move(showResult148.error()));
        
      }
    else
      {
        auto hideResult149 = pair.second->hide();
        if (!hideResult149)
          return std::unexpected(std::move(hideResult149.error()));
        
      }
  }

  for (auto& techOverlay : m_disabledTechOverlays)
    {
      auto setVisibilityResult150 = techOverlay->setVisibility(m_player->techOverridden());
      if (!setVisibilityResult150)
        return std::unexpected(std::move(setVisibilityResult150.error()));
      
    }

  auto fetchChildResult151 = fetchChild<LabelWidget>("healthtext");
  if (!fetchChildResult151)
    return std::unexpected(std::move(fetchChildResult151.error()));
  auto healthLabel = std::move(*fetchChildResult151);
  auto maxHealthResult152 = m_player->maxHealth();
  if (!maxHealthResult152)
    return std::unexpected(std::move(maxHealthResult152.error()));
  auto setTextResult153 = healthLabel->setText(Star::format("{:.1f}", std::move(*maxHealthResult152)));
  if (!setTextResult153)
    return std::unexpected(std::move(setTextResult153.error()));
  auto fetchChildResult154 = fetchChild<LabelWidget>("energytext");
  if (!fetchChildResult154)
    return std::unexpected(std::move(fetchChildResult154.error()));
  auto energyLabel = std::move(*fetchChildResult154);
  auto maxEnergyResult155 = m_player->maxEnergy();
  if (!maxEnergyResult155)
    return std::unexpected(std::move(maxEnergyResult155.error()));
  auto setTextResult156 = energyLabel->setText(Star::format("{:.1f}", std::move(*maxEnergyResult155)));
  if (!setTextResult156)
    return std::unexpected(std::move(setTextResult156.error()));
  auto fetchChildResult157 = fetchChild<LabelWidget>("weapontext");
  if (!fetchChildResult157)
    return std::unexpected(std::move(fetchChildResult157.error()));
  auto weaponLabel = std::move(*fetchChildResult157);
  auto powerMultiplierResult158 = m_player->powerMultiplier();
  if (!powerMultiplierResult158)
    return std::unexpected(std::move(powerMultiplierResult158.error()));
  auto setTextResult159 = weaponLabel->setText(Star::format("{:.1f}%", std::move(*powerMultiplierResult158) * 100));
  if (!setTextResult159)
    return std::unexpected(std::move(setTextResult159.error()));
  auto fetchChildResult160 = fetchChild<LabelWidget>("defensetext");
  if (!fetchChildResult160)
    return std::unexpected(std::move(fetchChildResult160.error()));
  auto defenseLabel = std::move(*fetchChildResult160);
  if (m_player->protection() == 0)
    {
      auto setTextResult161 = defenseLabel->setText("--");
      if (!setTextResult161)
        return std::unexpected(std::move(setTextResult161.error()));
      
    }
  else
    {
      auto setTextResult162 = defenseLabel->setText(Star::format("{:.1f}", m_player->protection()));
      if (!setTextResult162)
        return std::unexpected(std::move(setTextResult162.error()));
      
    }

  auto fetchChildResult163 = fetchChild<LabelWidget>("lblMoney");
  if (!fetchChildResult163)
    return std::unexpected(std::move(fetchChildResult163.error()));
  auto moneyLabel = std::move(*fetchChildResult163);
  auto setTextResult164 = moneyLabel->setText(Star::to_string(m_player->currency("money")));
  if (!setTextResult164)
    return std::unexpected(std::move(setTextResult164.error()));

  if (m_player->currency("essence") > 0) {
    auto fetchChildResult165 = fetchChild<ImageWidget>("imgEssenceIcon");
    if (!fetchChildResult165)
      return std::unexpected(std::move(fetchChildResult165.error()));
    auto showResult166 = std::move(*fetchChildResult165)->show();
    if (!showResult166)
      return std::unexpected(std::move(showResult166.error()));
    auto fetchChildResult167 = fetchChild<LabelWidget>("lblEssence");
    if (!fetchChildResult167)
      return std::unexpected(std::move(fetchChildResult167.error()));
    auto essenceLabel = std::move(*fetchChildResult167);
    auto showResult168 = essenceLabel->show();
    if (!showResult168)
      return std::unexpected(std::move(showResult168.error()));
    auto setTextResult169 = essenceLabel->setText(Star::to_string(m_player->currency("essence")));
    if (!setTextResult169)
      return std::unexpected(std::move(setTextResult169.error()));
  } else {
    auto fetchChildResult170 = fetchChild<ImageWidget>("imgEssenceIcon");
    if (!fetchChildResult170)
      return std::unexpected(std::move(fetchChildResult170.error()));
    auto hideResult171 = std::move(*fetchChildResult170)->hide();
    if (!hideResult171)
      return std::unexpected(std::move(hideResult171.error()));
    auto fetchChildResult172 = fetchChild<LabelWidget>("lblEssence");
    if (!fetchChildResult172)
      return std::unexpected(std::move(fetchChildResult172.error()));
    auto hideResult173 = std::move(*fetchChildResult172)->hide();
    if (!hideResult173)
      return std::unexpected(std::move(hideResult173.error()));
  }

  auto assetsResult174 = context.root().assets();
  if (!assetsResult174)
    return std::unexpected(std::move(assetsResult174.error()));
  auto jsonResult175 = std::move(*assetsResult174)->json("/interface/windowconfig/playerinventory.config");
  if (!jsonResult175)
    return std::unexpected(std::move(jsonResult175.error()));
  auto config = std::move(*jsonResult175);

  auto pets = m_player->companions()->getCompanions("pets");
  if (pets.size() > 0) {
    auto firstResult176 = pets.first();
    if (!firstResult176)
      return std::unexpected(std::move(firstResult176.error()));
    auto pet = firstResult176->get();
    auto fetchChildResult177 = fetchChild<ImageWidget>("companionSlot");
    if (!fetchChildResult177)
      return std::unexpected(std::move(fetchChildResult177.error()));
    auto companionImage = std::move(*fetchChildResult177);
    auto setVisibilityResult178 = companionImage->setVisibility(true);
    if (!setVisibilityResult178)
      return std::unexpected(std::move(setVisibilityResult178.error()));

    auto setDrawablesResult179 = companionImage->setDrawables(pet->portrait());
    if (!setDrawablesResult179)
      return std::unexpected(std::move(setDrawablesResult179.error()));
    auto fetchChildResult180 = fetchChild<LabelWidget>("companionName");
    if (!fetchChildResult180)
      return std::unexpected(std::move(fetchChildResult180.error()));
    auto nameLabel = std::move(*fetchChildResult180);
    {
      auto nameResult181 = pet->name();
      if (!nameResult181)
        return std::unexpected(std::move(nameResult181.error()));
      if (auto name = std::move(*nameResult181)) {
        auto nameResult182 = pet->name();
        if (!nameResult182)
          return std::unexpected(std::move(nameResult182.error()));
        auto toUpperResult183 = std::move(*nameResult182)->to_upper();
        if (!toUpperResult183)
          return std::unexpected(std::move(toUpperResult183.error()));
        auto setTextResult184 = nameLabel->setText(std::move(*toUpperResult183));
        if (!setTextResult184)
          return std::unexpected(std::move(setTextResult184.error()));
      } else {
        auto getStringResult185 = config.getString("defaultPetNameLabel");
        if (!getStringResult185)
          return std::unexpected(std::move(getStringResult185.error()));
        auto setTextResult186 = nameLabel->setText(std::move(*getStringResult185));
        if (!setTextResult186)
          return std::unexpected(std::move(setTextResult186.error()));
      }
    }

    auto fetchChildResult187 = fetchChild<LabelWidget>("companionAttackStat");
    if (!fetchChildResult187)
      return std::unexpected(std::move(fetchChildResult187.error()));
    auto attackLabel = std::move(*fetchChildResult187);
    {
      auto statResult188 = pet->stat("attack");
      if (!statResult188)
        return std::unexpected(std::move(statResult188.error()));
      if (auto attack = std::move(*statResult188)) {
        auto setTextResult189 = attackLabel->setText(Star::format("{:.1f}", *attack));
        if (!setTextResult189)
          return std::unexpected(std::move(setTextResult189.error()));
      } else {
        auto setTextResult190 = attackLabel->setText("");
        if (!setTextResult190)
          return std::unexpected(std::move(setTextResult190.error()));
      }
    }

    auto fetchChildResult191 = fetchChild<LabelWidget>("companionDefenseStat");
    if (!fetchChildResult191)
      return std::unexpected(std::move(fetchChildResult191.error()));
    auto defenseLabel = std::move(*fetchChildResult191);
    {
      auto statResult192 = pet->stat("defense");
      if (!statResult192)
        return std::unexpected(std::move(statResult192.error()));
      if (auto defense = std::move(*statResult192)) {
        auto setTextResult193 = defenseLabel->setText(Star::format("{:.1f}", *defense));
        if (!setTextResult193)
          return std::unexpected(std::move(setTextResult193.error()));
      } else {
        auto setTextResult194 = defenseLabel->setText("");
        if (!setTextResult194)
          return std::unexpected(std::move(setTextResult194.error()));
      }
    }

    {
      auto containsChildResult195 = containsChild("companionHealthBar");
      if (!containsChildResult195)
        return std::unexpected(std::move(containsChildResult195.error()));
      if (std::move(*containsChildResult195)) {
        auto fetchChildResult196 = fetchChild<ProgressWidget>("companionHealthBar");
        if (!fetchChildResult196)
          return std::unexpected(std::move(fetchChildResult196.error()));
        auto healthBar = std::move(*fetchChildResult196);
        auto resourceResult197 = pet->resource("health");
        if (!resourceResult197)
          return std::unexpected(std::move(resourceResult197.error()));
        std::optional<float> health = std::move(*resourceResult197);
        auto resourceMaxResult198 = pet->resourceMax("health");
        if (!resourceMaxResult198)
          return std::unexpected(std::move(resourceMaxResult198.error()));
        std::optional<float> healthMax = std::move(*resourceMaxResult198);
        if (health && healthMax) {
          healthBar->setCurrentProgressLevel(*health);
          healthBar->setMaxProgressLevel(*healthMax);
        } else {
          healthBar->setCurrentProgressLevel(0);
          healthBar->setMaxProgressLevel(1);
        }
      }
    }
  } else {
    auto fetchChildResult199 = fetchChild<ImageWidget>("companionSlot");
    if (!fetchChildResult199)
      return std::unexpected(std::move(fetchChildResult199.error()));
    auto setVisibilityResult200 = std::move(*fetchChildResult199)->setVisibility(false);
    if (!setVisibilityResult200)
      return std::unexpected(std::move(setVisibilityResult200.error()));

    auto fetchChildResult201 = fetchChild<LabelWidget>("companionName");
    if (!fetchChildResult201)
      return std::unexpected(std::move(fetchChildResult201.error()));
    auto getStringResult202 = config.getString("defaultPetNameLabel");
    if (!getStringResult202)
      return std::unexpected(std::move(getStringResult202.error()));
    auto setTextResult203 = std::move(*fetchChildResult201)->setText(std::move(*getStringResult202));
    if (!setTextResult203)
      return std::unexpected(std::move(setTextResult203.error()));

    auto fetchChildResult204 = fetchChild<LabelWidget>("companionAttackStat");
    if (!fetchChildResult204)
      return std::unexpected(std::move(fetchChildResult204.error()));
    auto setTextResult205 = std::move(*fetchChildResult204)->setText("");
    if (!setTextResult205)
      return std::unexpected(std::move(setTextResult205.error()));
    auto fetchChildResult206 = fetchChild<LabelWidget>("companionDefenseStat");
    if (!fetchChildResult206)
      return std::unexpected(std::move(fetchChildResult206.error()));
    auto setTextResult207 = std::move(*fetchChildResult206)->setText("");
    if (!setTextResult207)
      return std::unexpected(std::move(setTextResult207.error()));

    {
      auto containsChildResult208 = containsChild("companionHealthBar");
      if (!containsChildResult208)
        return std::unexpected(std::move(containsChildResult208.error()));
      if (std::move(*containsChildResult208)) {
        auto fetchChildResult209 = fetchChild<ProgressWidget>("companionHealthBar");
        if (!fetchChildResult209)
          return std::unexpected(std::move(fetchChildResult209.error()));
        auto healthBar = std::move(*fetchChildResult209);
        healthBar->setCurrentProgressLevel(0);
        healthBar->setMaxProgressLevel(1);
      }
    }
  }

  if (auto item = inventory->swapSlotItem()) {
    float pitch = 1.f - (static_cast<float>(item->count()) / static_cast<float>(item->maxStack())) * .2f;
    if (!m_currentSwapSlotItem || !item->matches(*m_currentSwapSlotItem, true))
      {
        auto randFromResult210 = RandomSource(context.root().random().randu64()).randFrom(m_pickUpSounds);
        if (!randFromResult210)
          return std::unexpected(std::move(randFromResult210.error()));
        auto playAudioResult211 = context.playAudio(randFromResult210->get(), 0, 1.f, pitch);
        if (!playAudioResult211)
          return std::unexpected(std::move(playAudioResult211.error()));
        
      }
    else if (item->count() > m_currentSwapSlotItem->count())
      {
        auto randFromResult212 = RandomSource(context.root().random().randu64()).randFrom(m_someUpSounds);
        if (!randFromResult212)
          return std::unexpected(std::move(randFromResult212.error()));
        auto playAudioResult213 = context.playAudio(randFromResult212->get(), 0, 1.f, pitch);
        if (!playAudioResult213)
          return std::unexpected(std::move(playAudioResult213.error()));
        
      }
    else if (item->count() < m_currentSwapSlotItem->count())
      {
        auto randFromResult214 = RandomSource(context.root().random().randu64()).randFrom(m_someDownSounds);
        if (!randFromResult214)
          return std::unexpected(std::move(randFromResult214.error()));
        auto playAudioResult215 = context.playAudio(randFromResult214->get(), 0, 1.f, pitch);
        if (!playAudioResult215)
          return std::unexpected(std::move(playAudioResult215.error()));
        
      }

    m_currentSwapSlotItem = item->descriptor();
  } else {
    if (m_currentSwapSlotItem)
      {
        auto randFromResult216 = RandomSource(context.root().random().randu64()).randFrom(m_putDownSounds);
        if (!randFromResult216)
          return std::unexpected(std::move(randFromResult216.error()));
        auto playAudioResult217 = context.playAudio(randFromResult216->get());
        if (!playAudioResult217)
          return std::unexpected(std::move(playAudioResult217.error()));
        
      }
    m_currentSwapSlotItem = {};
  }

  auto nameResult218 = m_player->name();
  if (!nameResult218)
    return std::unexpected(std::move(nameResult218.error()));
  m_title = std::move(*nameResult218);

  auto updateResult219 = Pane::update(dt);
  if (!updateResult219)
    return std::unexpected(std::move(updateResult219.error()));

return {};
}

std::expected<void, StarException> InventoryPane::selectTab(String const& selected) {
  for (auto grid : m_itemGrids)
    {
      auto hideResult220 = grid.second->hide();
      if (!hideResult220)
        return std::unexpected(std::move(hideResult220.error()));
      
    }
  m_selectedTab = selected;
  auto showResult221 = m_itemGrids[m_selectedTab]->show();
  if (!showResult221)
    return std::unexpected(std::move(showResult221.error()));
  m_itemGrids[m_selectedTab]->indicateChangedSlots();

  auto fetchChildResult222 = fetchChild<ButtonGroupWidget>("gridModeSelector");
  if (!fetchChildResult222)
    return std::unexpected(std::move(fetchChildResult222.error()));
  auto tabs = std::move(*fetchChildResult222);
  for (auto button : tabs->buttons())
    {
      auto toStringResult223 = button.get().data().toString();
      if (!toStringResult223)
        return std::unexpected(std::move(toStringResult223.error()));
      auto equalsIgnoreCaseResult224 = std::move(*toStringResult223).equals_ignore_case(m_tabButtonData[selected]);
      if (!equalsIgnoreCaseResult224)
        return std::unexpected(std::move(equalsIgnoreCaseResult224.error()));
      if (std::move(*equalsIgnoreCaseResult224))
        {
          auto selectResult225 = tabs->select(tabs->id(button));
          if (!selectResult225)
            return std::unexpected(std::move(selectResult225.error()));
          
        }
    }

return {};
}

}
