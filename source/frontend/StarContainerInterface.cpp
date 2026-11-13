import star.json_extra;
import std;
import star.json;
import star.xx_hash;
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
import star.lua;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.format;
import star.version;
import star.image;
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
import star.periodic_function;
import star.periodic;

import star.lua_root;

import star.lua_components;
import star.lua_actor_movement_component;
import star.lua_animation_component;
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

import star.container_entity;


import star.container_interactor;
import star.widget_parsing;
import star.gui_reader;


import star.container_interface;
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
import star.world_client_state;
import star.interpolation_tracker;
import star.ambient;
import star.weather;
import star.world_client;
import star.progress_widget;
import star.animation;
import star.item_slot_widget;
import star.item_grid_widget;
import star.label_widget;
import star.image_widget;
import star.pane_manager;
import star.fuel_widget;
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
import star.radio_message_database;
import star.player;
import star.status_effect_entity;
import star.scripted_entity;
import star.wire_entity;
import star.object;
import star.widget_lua_bindings;
import star.augment_item;
import star.input;

import star.player_lua_bindings;
import star.item_tooltip;
import star.config_lua_bindings;
import star.status_controller_lua_bindings;


import star.player_inventory;
import star.item_bag;
import star.optional;

namespace Star {

ContainerPane::ContainerPane(GuiContext& guiContext) : Pane(guiContext), m_reader(guiContext) {}

std::expected<std::shared_ptr<ContainerPane>, StarException> ContainerPane::create(GuiContext& guiContext, std::shared_ptr<WorldClient> worldClient, std::shared_ptr<Player> player, std::shared_ptr<ContainerInteractor> containerInteractor) {
  auto pane = std::shared_ptr<ContainerPane>(new ContainerPane(guiContext));
  auto base = pane->initializePane();
  if (!base)
    return std::unexpected(std::move(base.error()));
  auto initialized = pane->initialize(std::move(worldClient), std::move(player), std::move(containerInteractor));
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return pane;
}

std::expected<void, StarException> ContainerPane::initialize(std::shared_ptr<WorldClient> worldClient, std::shared_ptr<Player> player, std::shared_ptr<ContainerInteractor> containerInteractor) {
  m_worldClient = std::move(worldClient);
  m_player = std::move(player);
  m_containerInteractor = std::move(containerInteractor);

  auto container = m_containerInteractor->openContainer();

  if (!container)
    return std::unexpected(StarException("Tried to instantiate a ContainerPane with no opened container!"));

  auto containerGuiConfigResult226 = container->containerGuiConfig();
  if (!containerGuiConfigResult226)
    return std::unexpected(std::move(containerGuiConfigResult226.error()));
  auto guiConfig = std::move(*containerGuiConfigResult226);

  auto optResult227 = guiConfig.opt("scripts");
  if (!optResult227)
    return std::unexpected(std::move(optResult227.error()));
  auto scriptsJson = std::move(*optResult227);
  if (scriptsJson) {
    auto jsonToStringListResult228 = jsonToStringList(*scriptsJson);
    if (!jsonToStringListResult228)
      return std::unexpected(std::move(jsonToStringListResult228.error()));
    auto scripts = std::move(*jsonToStringListResult228);
    if (!m_script) {
      m_script.emplace(context().root());
      auto setScriptsResult229 = m_script->setScripts(scripts);
      if (!setScriptsResult229)
        return std::unexpected(std::move(setScriptsResult229.error()));
    }
    auto makeWidgetCallbacksResult230 = LuaBindings::makeWidgetCallbacks(context(), *this);
    if (!makeWidgetCallbacksResult230)
      return std::unexpected(std::move(makeWidgetCallbacksResult230.error()));
    auto addCallbacksResult231 = m_script->addCallbacks("widget", std::move(*makeWidgetCallbacksResult230));
    if (!addCallbacksResult231)
      return std::unexpected(std::move(addCallbacksResult231.error()));
    auto makeConfigCallbacksResult233 = LuaBindings::makeConfigCallbacks( [guiConfig](String const& name, Json const& def)  -> std::expected<Json, StarException> {
            auto queryResult232 = guiConfig.query(name, def);
            if (!queryResult232)
              return std::unexpected(std::move(queryResult232.error()));
            return std::move(*queryResult232);
          });
    if (!makeConfigCallbacksResult233)
      return std::unexpected(std::move(makeConfigCallbacksResult233.error()));
    auto addCallbacksResult234 = m_script->addCallbacks("config", std::move(*makeConfigCallbacksResult233));
    if (!addCallbacksResult234)
      return std::unexpected(std::move(addCallbacksResult234.error()));
    auto makePlayerCallbacksResult235 = LuaBindings::makePlayerCallbacks(context().root(), *m_player);
    if (!makePlayerCallbacksResult235)
      return std::unexpected(std::move(makePlayerCallbacksResult235.error()));
    auto addCallbacksResult236 = m_script->addCallbacks("player", std::move(*makePlayerCallbacksResult235));
    if (!addCallbacksResult236)
      return std::unexpected(std::move(addCallbacksResult236.error()));
    auto makeStatusControllerCallbacksResult237 = LuaBindings::makeStatusControllerCallbacks(m_player->statusController());
    if (!makeStatusControllerCallbacksResult237)
      return std::unexpected(std::move(makeStatusControllerCallbacksResult237.error()));
    auto addCallbacksResult238 = m_script->addCallbacks("status", std::move(*makeStatusControllerCallbacksResult237));
    if (!addCallbacksResult238)
      return std::unexpected(std::move(addCallbacksResult238.error()));

    LuaCallbacks containerPaneCallbacks;
    auto registerCallbackResult239 = containerPaneCallbacks.registerCallback("containerEntityId", [this]() -> std::optional<EntityId> {
            return m_containerInteractor->openContainerId();
          });
    if (!registerCallbackResult239)
      return std::unexpected(std::move(registerCallbackResult239.error()));
    auto registerCallbackResult240 = containerPaneCallbacks.registerCallback("playerEntityId", [this]() { return m_player->entityId(); });
    if (!registerCallbackResult240)
      return std::unexpected(std::move(registerCallbackResult240.error()));
    auto registerCallbackResult241 = containerPaneCallbacks.registerCallback("dismiss", [this]()  -> std::expected<void, StarException> { dismiss(); 
      return {};
    });
    if (!registerCallbackResult241)
      return std::unexpected(std::move(registerCallbackResult241.error()));
    auto addCallbacksResult242 = m_script->addCallbacks("pane", containerPaneCallbacks);
    if (!addCallbacksResult242)
      return std::unexpected(std::move(addCallbacksResult242.error()));

    auto getUIntResult243 = guiConfig.getUInt("scriptDelta", 1);
    if (!getUIntResult243)
      return std::unexpected(std::move(getUIntResult243.error()));
    m_script->setUpdateDelta(std::move(*getUIntResult243));
  }

  auto rightClickCallback = [this](std::size_t index)  -> std::expected<void, StarException> {
    if (m_expectingSwap != ExpectingSwap::None)
      return {};

    auto callbackContainer = m_containerInteractor->openContainer();

    if (!callbackContainer) {
      auto warnResult244 = context().root().logger().warn("rightClickCallback failed in ContainerPane because we do not have an open container!");
      if (!warnResult244)
        return std::unexpected(std::move(warnResult244.error()));
      return {};
    }

    {
      auto atResult245 = callbackContainer->itemBag()->at(index);
      if (!atResult245)
        return std::unexpected(std::move(atResult245.error()));
      if (std::shared_ptr<Item> slotItem = std::move(*atResult245)) {
        auto swapItem = m_player->inventory()->swapSlotItem();
        if (!swapItem || swapItem->empty() || swapItem->couldStack(optionalRef(slotItem))) {
          std::size_t count = swapItem ? swapItem->couldStack(optionalRef(slotItem)) : slotItem->maxStack();
          if (context().shiftHeld())
            count = std::max<std::uint64_t>(1, std::min<std::uint64_t>(count, slotItem->count() / 2));
          else
            count = 1;
  
          auto takeFromContainerSlotResult246 = m_containerInteractor->takeFromContainerSlot(index, count);
          if (!takeFromContainerSlotResult246)
            return std::unexpected(std::move(takeFromContainerSlotResult246.error()));
          m_expectingSwap = ExpectingSwap::SwapSlotStack;
        } else if (is<AugmentItem>(swapItem)) {
          auto applyAugmentInContainerResult247 = m_containerInteractor->applyAugmentInContainer(index, swapItem);
          if (!applyAugmentInContainerResult247)
            return std::unexpected(std::move(applyAugmentInContainerResult247.error()));
          auto setSwapSlotItemResult248 = m_player->inventory()->setSwapSlotItem({});
          if (!setSwapSlotItemResult248)
            return std::unexpected(std::move(setSwapSlotItemResult248.error()));
          m_expectingSwap = ExpectingSwap::SwapSlot;
        }
      }
    }
  
  return {};
};

  m_reader.registerCallback("close", [this](Widget&)  -> std::expected<void, StarException> { dismiss();
  return {};
});
  m_reader.registerCallback("itemGrid", [this](Widget& paneObj) -> std::expected<void, StarException> {
    if (auto itemGrid = as_ref<ItemGridWidget>(paneObj)) {
      auto swapSlotResult249 = swapSlot(itemGrid->get());
      if (!swapSlotResult249)
        return std::unexpected(std::move(swapSlotResult249.error()));

    } else
      return std::unexpected(GuiException("Invalid object type, expected ItemGridWidget."));

    return {};
  });
  m_reader.registerCallback("itemGrid.right",
                            [rightClickCallback](Widget& paneObj) -> std::expected<void, StarException> {
                              if (auto itemGrid = as_ref<ItemGridWidget>(paneObj)) {
                                auto rightClickCallbackResult250 = rightClickCallback(itemGrid->get().selectedIndex());
                                if (!rightClickCallbackResult250)
                                  return std::unexpected(std::move(rightClickCallbackResult250.error()));

                              } else
                                return std::unexpected(GuiException("Invalid object type, expected ItemGridWidget."));

                              return {};
                            });

  m_reader.registerCallback("itemGrid2", [this](Widget& paneObj) -> std::expected<void, StarException> {
    if (auto itemGrid = as_ref<ItemGridWidget>(paneObj)) {
      auto swapSlotResult251 = swapSlot(itemGrid->get());
      if (!swapSlotResult251)
        return std::unexpected(std::move(swapSlotResult251.error()));

    } else
      return std::unexpected(GuiException("Invalid object type, expected ItemGridWidget."));

    return {};
  });
  m_reader.registerCallback("itemGrid2.right",
                            [rightClickCallback](Widget& paneObj) -> std::expected<void, StarException> {
                              if (auto itemGrid = as_ref<ItemGridWidget>(paneObj)) {
                                auto rightClickCallbackResult252 = rightClickCallback(itemGrid->get().selectedIndex());
                                if (!rightClickCallbackResult252)
                                  return std::unexpected(std::move(rightClickCallbackResult252.error()));

                              } else
                                return std::unexpected(GuiException("Invalid object type, expected ItemGridWidget."));

                              return {};
                            });

  m_reader.registerCallback("outputItemGrid", [this](Widget& paneObj) -> std::expected<void, StarException> {
    if (auto itemGrid = as_ref<ItemGridWidget>(paneObj)) {
      auto swapSlotResult253 = swapSlot(itemGrid->get());
      if (!swapSlotResult253)
        return std::unexpected(std::move(swapSlotResult253.error()));

    } else
      return std::unexpected(GuiException("Invalid object type, expected ItemGridWidget."));

    return {};
  });
  m_reader.registerCallback("outputItemGrid.right",
                            [rightClickCallback](Widget& paneObj) -> std::expected<void, StarException> {
                              if (auto itemGrid = as_ref<ItemGridWidget>(paneObj)) {
                                auto rightClickCallbackResult254 = rightClickCallback(itemGrid->get().selectedIndex());
                                if (!rightClickCallbackResult254)
                                  return std::unexpected(std::move(rightClickCallbackResult254.error()));

                              } else
                                return std::unexpected(GuiException("Invalid object type, expected ItemGridWidget."));

                              return {};
                            });

  m_reader.registerCallback("toggleCrafting", [this](Widget&)  -> std::expected<void, StarException> { auto toggleCraftingResult255 = toggleCrafting();
                                                                if (!toggleCraftingResult255)
                                                                  return std::unexpected(std::move(toggleCraftingResult255.error())); 
  return {};
});

  m_reader.registerCallback("clear", [this](Widget&)  -> std::expected<void, StarException> { auto clearResult256 = clear();
                                                       if (!clearResult256)
                                                         return std::unexpected(std::move(clearResult256.error())); 
  return {};
});
  m_reader.registerCallback("burn", [this](Widget&)  -> std::expected<void, StarException> { auto burnResult257 = burn();
                                                      if (!burnResult257)
                                                        return std::unexpected(std::move(burnResult257.error())); 
  return {};
});

  {
    auto getResult258 = guiConfig.get("scriptWidgetCallbacks", JsonArray{});
    if (!getResult258)
      return std::unexpected(std::move(getResult258.error()));
    auto jsonToStringListResult259 = jsonToStringList(std::move(*getResult258));
    if (!jsonToStringListResult259)
      return std::unexpected(std::move(jsonToStringListResult259.error()));
    for (auto const& callbackName : std::move(*jsonToStringListResult259)) {
      m_reader.registerCallback(callbackName, [this, callbackName](Widget& widget)  -> std::expected<void, StarException> {
        auto invokeResult260 = recoverLuaScriptError(m_script->invoke(callbackName, widget.name(), widget.data()));
        if (!invokeResult260)
          return std::unexpected(std::move(invokeResult260.error()));
      
    return {};
  });
    }
  }

  auto getResult261 = guiConfig.get("gui");
  if (!getResult261)
    return std::unexpected(std::move(getResult261.error()));
  auto constructResult262 = m_reader.construct(std::move(*getResult261), *this);
  if (!constructResult262)
    return std::unexpected(std::move(constructResult262.error()));

  {
    auto fetchChildResult263 = fetchChild<LabelWidget>("count");
    if (!fetchChildResult263)
      return std::unexpected(std::move(fetchChildResult263.error()));
    if (auto countWidget = std::move(*fetchChildResult263))
      {
        auto replaceResult264 = countWidget->text().replace("<slots>", Star::to_string(container->containerSize()));
        if (!replaceResult264)
          return std::unexpected(std::move(replaceResult264.error()));
        auto setTextResult265 = countWidget->setText(std::move(*replaceResult264));
        if (!setTextResult265)
          return std::unexpected(std::move(setTextResult265.error()));
        
      }
  }

  m_itemBag = std::make_shared<ItemBag>(context().root(), container->containerSize());
  auto items = container->containerItems();

  auto fetchChildResult266 = fetchChild<ItemGridWidget>("itemGrid");
  if (!fetchChildResult266)
    return std::unexpected(std::move(fetchChildResult266.error()));
  auto setItemBagResult267 = std::move(*fetchChildResult266)->setItemBag(m_itemBag);
  if (!setItemBagResult267)
    return std::unexpected(std::move(setItemBagResult267.error()));
  {
    auto containsChildResult268 = containsChild("itemGrid2");
    if (!containsChildResult268)
      return std::unexpected(std::move(containsChildResult268.error()));
    if (std::move(*containsChildResult268))
      {
        auto fetchChildResult269 = fetchChild<ItemGridWidget>("itemGrid2");
        if (!fetchChildResult269)
          return std::unexpected(std::move(fetchChildResult269.error()));
        auto setItemBagResult270 = std::move(*fetchChildResult269)->setItemBag(m_itemBag);
        if (!setItemBagResult270)
          return std::unexpected(std::move(setItemBagResult270.error()));
        
      }
  }
  {
    auto containsChildResult271 = containsChild("outputItemGrid");
    if (!containsChildResult271)
      return std::unexpected(std::move(containsChildResult271.error()));
    if (std::move(*containsChildResult271))
      {
        auto fetchChildResult272 = fetchChild<ItemGridWidget>("outputItemGrid");
        if (!fetchChildResult272)
          return std::unexpected(std::move(fetchChildResult272.error()));
        auto setItemBagResult273 = std::move(*fetchChildResult272)->setItemBag(m_itemBag);
        if (!setItemBagResult273)
          return std::unexpected(std::move(setItemBagResult273.error()));
        
      }
  }

  {
    auto iconItemResult274 = container->iconItem();
    if (!iconItemResult274)
      return std::unexpected(std::move(iconItemResult274.error()));
    if (std::move(*iconItemResult274)) {
      auto itemDatabaseResult275 = context().root().itemDatabase();
      if (!itemDatabaseResult275)
        return std::unexpected(std::move(itemDatabaseResult275.error()));
      auto itemDatabase = std::move(*itemDatabaseResult275);
      auto iconItemResult276 = container->iconItem();
      if (!iconItemResult276)
        return std::unexpected(std::move(iconItemResult276.error()));
      auto itemSharedResult277 = itemDatabase->itemShared(std::move(*iconItemResult276));
      if (!itemSharedResult277)
        return std::unexpected(std::move(itemSharedResult277.error()));
      auto iconItem = std::move(*itemSharedResult277);
      auto createResult278 = ItemSlotWidget::create(context(), iconItem, "/interface/inventory/portrait.png");
      if (!createResult278)
        return std::unexpected(std::move(createResult278.error()));
      auto icon = std::move(*createResult278);
      icon->showDurability(false);
      icon->showRarity(false);
      icon->setBackingImageAffinity(true, true);
      auto containerDescriptionResult279 = container->containerDescription();
      if (!containerDescriptionResult279)
        return std::unexpected(std::move(containerDescriptionResult279.error()));
      auto containerSubTitleResult280 = container->containerSubTitle();
      if (!containerSubTitleResult280)
        return std::unexpected(std::move(containerSubTitleResult280.error()));
      auto setTitleResult281 = setTitle(icon, std::move(*containerDescriptionResult279), std::move(*containerSubTitleResult280));
      if (!setTitleResult281)
        return std::unexpected(std::move(setTitleResult281.error()));
    }
  }

  {
    auto containsChildResult282 = containsChild("objectImage");
    if (!containsChildResult282)
      return std::unexpected(std::move(containsChildResult282.error()));
    if (std::move(*containsChildResult282))
      if (auto containerObject = as<Object>(container))
        {
          auto fetchChildResult283 = fetchChild<ImageWidget>("objectImage");
          if (!fetchChildResult283)
            return std::unexpected(std::move(fetchChildResult283.error()));
          auto cursorHintDrawablesResult284 = containerObject->cursorHintDrawables();
          if (!cursorHintDrawablesResult284)
            return std::unexpected(std::move(cursorHintDrawablesResult284.error()));
          auto setDrawablesResult285 = std::move(*fetchChildResult283)->setDrawables(std::move(*cursorHintDrawablesResult284));
          if (!setDrawablesResult285)
            return std::unexpected(std::move(setDrawablesResult285.error()));
          
        }
  }

  m_expectingSwap = ExpectingSwap::None;
  return {};
}

std::expected<void, StarException> ContainerPane::displayed() {
  auto displayedResult286 = Pane::displayed();
  if (!displayedResult286)
    return std::unexpected(std::move(displayedResult286.error()));

  m_expectingSwap = ExpectingSwap::None;

  if (m_script) {
    if (m_worldClient && m_worldClient->inWorld())
      {
        auto initResult287 = recoverLuaScriptError(m_script->init(*m_worldClient));
        if (!initResult287)
          return std::unexpected(std::move(initResult287.error()));
        
      }

    auto invokeResult288 = recoverLuaScriptError(m_script->invoke("displayed"));
    if (!invokeResult288)
      return std::unexpected(std::move(invokeResult288.error()));
  }
  return {};
}

std::expected<void, StarException> ContainerPane::dismissed() {
  auto dismissedResult289 = Pane::dismissed();
  if (!dismissedResult289)
    return std::unexpected(std::move(dismissedResult289.error()));

  if (m_script) {
    auto invokeResult290 = recoverLuaScriptError(m_script->invoke("dismissed"));
    if (!invokeResult290)
      return std::unexpected(std::move(invokeResult290.error()));
    auto uninitResult291 = recoverLuaScriptError(m_script->uninit());
    if (!uninitResult291)
      return std::unexpected(std::move(uninitResult291.error()));
  }
  return {};
}

std::expected<bool, StarException> ContainerPane::giveContainerResult(ContainerResult result) {
  if (m_expectingSwap == ExpectingSwap::None)
    return false;

  for (auto item : result) {
    auto inv = m_player->inventory();
    auto triggerPickupEventsResult292 = m_player->triggerPickupEvents(item);
    if (!triggerPickupEventsResult292)
      return std::unexpected(std::move(triggerPickupEventsResult292.error()));

    if (m_expectingSwap == ExpectingSwap::SwapSlot) {
      auto clearSwapResult293 = m_player->clearSwap();
      if (!clearSwapResult293)
        return std::unexpected(std::move(clearSwapResult293.error()));
      auto setSwapSlotItemResult294 = inv->setSwapSlotItem(item);
      if (!setSwapSlotItemResult294)
        return std::unexpected(std::move(setSwapSlotItemResult294.error()));
    } else if (m_expectingSwap == ExpectingSwap::SwapSlotStack) {
      auto swapItem = inv->swapSlotItem();
      if (swapItem && swapItem->stackWith(optionalRef(item))) {
        continue;
      } else {
        auto clearSwapResult295 = inv->clearSwap();
        if (!clearSwapResult295)
          return std::unexpected(std::move(clearSwapResult295.error()));
        auto setSwapSlotItemResult296 = inv->setSwapSlotItem(item);
        if (!setSwapSlotItemResult296)
          return std::unexpected(std::move(setSwapSlotItemResult296.error()));
      }
    } else {
      auto addItemsResult297 = inv->addItems(item);
      if (!addItemsResult297)
        return std::unexpected(std::move(addItemsResult297.error()));
      auto addToContainerResult298 = m_containerInteractor->addToContainer(std::move(*addItemsResult297));
      if (!addToContainerResult298)
        return std::unexpected(std::move(addToContainerResult298.error()));
    }
  }

  m_expectingSwap = ExpectingSwap::None;
  return true;
}

std::expected<std::shared_ptr<Pane>, StarException> ContainerPane::createTooltip(Vec2I const& screenPosition) {
  std::shared_ptr<Item> item;
  if (auto child = getChildAt(screenPosition)) {
    if (auto itemSlot = as<ItemSlotWidget>(child))
      item = itemSlot->item();
    if (auto itemGrid = as<ItemGridWidget>(child))
      {
        auto itemAtResult299 = itemGrid->itemAt(screenPosition);
        if (!itemAtResult299)
          return std::unexpected(std::move(itemAtResult299.error()));
        item = std::move(*itemAtResult299);
      }
  }
  if (item)
    {
      auto buildItemTooltipResult300 = ItemTooltipBuilder::buildItemTooltip(context(), item, m_player);
      if (!buildItemTooltipResult300)
        return std::unexpected(std::move(buildItemTooltipResult300.error()));
      return std::move(*buildItemTooltipResult300);
    }
  return {};
}

std::expected<void, StarException> ContainerPane::swapSlot(ItemGridWidget& grid) {
  auto inv = m_player->inventory();
  if (context().shiftHeld()) {
    auto selectedItemResult301 = grid.selectedItem();
    if (!selectedItemResult301)
      return std::unexpected(std::move(selectedItemResult301.error()));
    auto containerItem = std::move(*selectedItemResult301);
    {
      bool condition303 = static_cast<bool>(containerItem);
      if (condition303) {
        auto itemsCanFitResult302 = inv->itemsCanFit(containerItem);
        if (!itemsCanFitResult302)
          return std::unexpected(std::move(itemsCanFitResult302.error()));
        condition303 = static_cast<bool>(std::move(*itemsCanFitResult302) >= containerItem->count());
      }
      if (condition303) {
        auto swapInContainerResult304 = m_containerInteractor->swapInContainer(grid.selectedIndex(), {});
        if (!swapInContainerResult304)
          return std::unexpected(std::move(swapInContainerResult304.error()));
        m_expectingSwap = ExpectingSwap::Inventory;
      }
    }
  } else {
    auto swapInContainerResult305 = m_containerInteractor->swapInContainer(grid.selectedIndex(), inv->swapSlotItem());
    if (!swapInContainerResult305)
      return std::unexpected(std::move(swapInContainerResult305.error()));
    auto setSwapSlotItemResult306 = inv->setSwapSlotItem({});
    if (!setSwapSlotItemResult306)
      return std::unexpected(std::move(setSwapSlotItemResult306.error()));
    m_expectingSwap = ExpectingSwap::SwapSlot;
  }
  return {};
}

std::expected<void, StarException> ContainerPane::startCrafting() {
  auto startCraftingInContainerResult307 = m_containerInteractor->startCraftingInContainer();
  if (!startCraftingInContainerResult307)
    return std::unexpected(std::move(startCraftingInContainerResult307.error()));

return {};
}

std::expected<void, StarException> ContainerPane::stopCrafting() {
  auto stopCraftingInContainerResult308 = m_containerInteractor->stopCraftingInContainer();
  if (!stopCraftingInContainerResult308)
    return std::unexpected(std::move(stopCraftingInContainerResult308.error()));

return {};
}

std::expected<void, StarException> ContainerPane::toggleCrafting() {
  if (auto container = m_containerInteractor->openContainer())
    if (container->isCrafting())
      {
        auto stopCraftingResult309 = stopCrafting();
        if (!stopCraftingResult309)
          return std::unexpected(std::move(stopCraftingResult309.error()));
        
      }
    else
      {
        auto startCraftingResult310 = startCrafting();
        if (!startCraftingResult310)
          return std::unexpected(std::move(startCraftingResult310.error()));
        
      }
  else
    {
      auto warnResult311 = context().root().logger().warn("ContainerPane::toggleCrafting failed because we don't have an open container!");
      if (!warnResult311)
        return std::unexpected(std::move(warnResult311.error()));
      
    }

return {};
}

std::expected<void, StarException> ContainerPane::clear() {
  auto clearContainerResult312 = m_containerInteractor->clearContainer();
  if (!clearContainerResult312)
    return std::unexpected(std::move(clearContainerResult312.error()));

return {};
}

std::expected<void, StarException> ContainerPane::burn() {
  auto burnContainerResult313 = m_containerInteractor->burnContainer();
  if (!burnContainerResult313)
    return std::unexpected(std::move(burnContainerResult313.error()));

return {};
}

std::expected<void, StarException> ContainerPane::update(float dt) {
  auto updateResult314 = Pane::update(dt);
  if (!updateResult314)
    return std::unexpected(std::move(updateResult314.error()));

  if (m_script)
    {
      auto updateResult315 = recoverLuaScriptError(m_script->update(m_script->updateDt(dt)));
      if (!updateResult315)
        return std::unexpected(std::move(updateResult315.error()));
      
    }

  m_itemBag->clearItems();
  auto inputResult316 = context().input();
  if (!inputResult316)
    return std::unexpected(std::move(inputResult316.error()));
  Input& input = inputResult316->get();

  if (!m_containerInteractor->containerOpen()) {
    dismiss();
    return {};
  }
  
  auto container = m_containerInteractor->openContainer();

  for (std::size_t i = 0; i < m_itemBag->size(); ++i)
    {
      auto putItemsResult317 = m_itemBag->putItems(i, container->containerItems()[i]);
      if (!putItemsResult317)
        return std::unexpected(std::move(putItemsResult317.error()));
      
    }

  if (container->isInteractive()) {
    {
      auto fetchChildResult318 = fetchChild<ItemGridWidget>("itemGrid");
      if (!fetchChildResult318)
        return std::unexpected(std::move(fetchChildResult318.error()));
      if (auto itemGrid = std::move(*fetchChildResult318)) {
        auto setProgressResult319 = itemGrid->setProgress(container->craftingProgress());
        if (!setProgressResult319)
          return std::unexpected(std::move(setProgressResult319.error()));
        auto updateAllItemSlotsResult320 = itemGrid->updateAllItemSlots();
        if (!updateAllItemSlotsResult320)
          return std::unexpected(std::move(updateAllItemSlotsResult320.error()));
      }
    }
    {
      auto fetchChildResult321 = fetchChild<ItemGridWidget>("itemGrid2");
      if (!fetchChildResult321)
        return std::unexpected(std::move(fetchChildResult321.error()));
      if (auto itemGrid = std::move(*fetchChildResult321)) {
        auto setProgressResult322 = itemGrid->setProgress(container->craftingProgress());
        if (!setProgressResult322)
          return std::unexpected(std::move(setProgressResult322.error()));
        auto updateAllItemSlotsResult323 = itemGrid->updateAllItemSlots();
        if (!updateAllItemSlotsResult323)
          return std::unexpected(std::move(updateAllItemSlotsResult323.error()));
      }
    }

    {
      auto fetchChildResult324 = fetchChild<FuelWidget>("fuelGauge");
      if (!fetchChildResult324)
        return std::unexpected(std::move(fetchChildResult324.error()));
      if (auto fuelGauge = std::move(*fetchChildResult324)) {
        auto getPropertyResult325 = m_worldClient->getProperty("ship.fuel", 0);
        if (!getPropertyResult325)
          return std::unexpected(std::move(getPropertyResult325.error()));
        auto toUIntResult326 = std::move(*getPropertyResult325).toUInt();
        if (!toUIntResult326)
          return std::unexpected(std::move(toUIntResult326.error()));
        fuelGauge->setCurrentFuelLevel(std::move(*toUIntResult326));
        auto getPropertyResult327 = m_worldClient->getProperty("ship.maxFuel", 0);
        if (!getPropertyResult327)
          return std::unexpected(std::move(getPropertyResult327.error()));
        auto toUIntResult328 = std::move(*getPropertyResult327).toUInt();
        if (!toUIntResult328)
          return std::unexpected(std::move(toUIntResult328.error()));
        fuelGauge->setMaxFuelLevel(std::move(*toUIntResult328));
        float totalFuelAmount = 0;
        for (auto& item : container->containerItems()) {
          if (item)
            {
              auto instanceValueResult329 = item->instanceValue("fuelAmount", 0);
              if (!instanceValueResult329)
                return std::unexpected(std::move(instanceValueResult329.error()));
              auto toUIntResult330 = std::move(*instanceValueResult329).toUInt();
              if (!toUIntResult330)
                return std::unexpected(std::move(toUIntResult330.error()));
              totalFuelAmount += std::move(*toUIntResult330) * item->count();
            }
        }
        fuelGauge->setPotentialFuelAmount(totalFuelAmount);
        fuelGauge->setRequestedFuelAmount(0);
      }
    }
  
    if (input.bindDown("opensb", "takeAll")) {
      auto clearContainerResult331 = m_containerInteractor->clearContainer();
      if (!clearContainerResult331)
        return std::unexpected(std::move(clearContainerResult331.error()));
    }
  }
  return {};
}

}
