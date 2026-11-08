import star.json_extra;
import std;
import star.memory;
import star.json;
import star.xx_hash;
import star.id_map;
import star.data_stream;
import star.bi_map;
import star.game_types;
import star.image;
import star.math_common;
import star.either;
import star.random;
import star.weighted_pool;
import star.strong_typedef;
import star.ordered_map;
import star.lru_cache;
import star.thread;
import star.time;
import star.ttl_cache;
import star.perlin;
import star.vector;
import star.random_point;
import star.casting;
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
import star.rpc_promise;
import star.format;
import star.version;
import star.lua;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.net_compatibility;
import star.worker_pool;

import star.sector_array_2d;
import star.btree;
import star.data_stream_devices;
import star.btree_database;
import star.ordered_set;
import star.a_star;
import star.periodic_function;

import star.item;
import star.asset_source;
import star.assets;
import star.root_base;
import star.root;
import star.tile_sector_array;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.world_layout;
import star.celestial_types;
import star.chat_types;
import star.uuid;
import star.tile_modification;
import star.warping;
import star.wiring;
import star.inspectable_entity;
import star.plant;
import star.biome_placement;
import star.lua_root;
import star.versioning_database;
import star.world_storage;
import star.player_types;
import star.system_world;
import star.damage_manager;
import star.net_packets;
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
import star.application;
import star.statistics_service;
import star.p2p_networking_service;
import star.user_generated_content_service;
import star.desktop_service;
import star.application_controller;
import star.renderer;
import star.item_descriptor;
import star.item_recipe;

import star.world_geometry;
import star.collision_block;
import star.liquid_types;
import star.tile_damage;
import star.collision_generator;
import star.world_tiles;
import star.entity_rendering_types;
import star.sky_types;
import star.celestial_coordinate;
import star.sky_parameters;
import star.sky_render_data;
import star.plant_database;
import star.parallax;
import star.particle;
import star.weather_types;
import star.damage_types;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.cellular_light_array;
import star.cellular_lighting;
import star.world_render_data;
import star.material_render_profile;
import star.tile_drawer;
import star.tile_painter;
import star.environment_painter;
import star.world_camera;
import star.world_painter;
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


import star.crafting_interface;
import star.widget_parsing;
import star.gui_reader;
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
import star.inventory_types;
import star.movement_controller;
import star.platformer_astar_types;
import star.actor_movement_controller;
import star.ai_types;
import star.lua_actor_movement_component;
import star.lua_animation_component;
import star.radio_message_database;
import star.player;
import star.interaction_types;
import star.quest_descriptor;
import star.interactive_entity;
import star.tile_entity;
import star.container_entity;
import star.player_blueprints;
import star.button_group;
import star.button_widget;
import star.game_timers;
import star.pane_manager;
import star.portrait_widget;
import star.label_widget;
import star.text_box_widget;
import star.image_widget;
import star.list_widget;
import star.image_stretch_widget;
import star.progress_widget;
import star.animation;
import star.item_slot_widget;
import star.configuration;
import star.player_log;

import star.object_item;
import star.item_tooltip;
import star.item_database;
import star.object_database;


import star.player_inventory;

import star.algorithm;
namespace Star {

CraftingPane::CraftingPane(GuiContext& guiContext) : Pane(guiContext) {}

std::expected<std::shared_ptr<CraftingPane>, StarException> CraftingPane::create(GuiContext& guiContext, std::shared_ptr<WorldClient> worldClient, std::shared_ptr<Player> player, Json const& settings, EntityId sourceEntityId) {
  auto pane = std::shared_ptr<CraftingPane>(new CraftingPane(guiContext));
  auto base = pane->initializePane();
  if (!base)
    return std::unexpected(std::move(base.error()));
  auto initialized = pane->initialize(std::move(worldClient), std::move(player), settings, sourceEntityId);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return pane;
}

std::expected<void, StarException> CraftingPane::initialize(std::shared_ptr<WorldClient> worldClient, std::shared_ptr<Player> player, Json const& settings, EntityId sourceEntityId) {
  m_worldClient = std::move(worldClient);
  m_player = std::move(player);
  m_blueprints = m_player->blueprints();
  m_recipeAutorefreshCooldown = 0;
  m_sourceEntityId = sourceEntityId;

  auto assetsResult332 = context().root().assets();
  if (!assetsResult332)
    return std::unexpected(std::move(assetsResult332.error()));
  auto assets = std::move(*assetsResult332);
  // get the config data for this crafting pane, default to "bare hands" crafting
  auto getResult333 = settings.get("config", "/interface/windowconfig/crafting.config");
  if (!getResult333)
    return std::unexpected(std::move(getResult333.error()));
  auto baseConfig = std::move(*getResult333);
  auto jsonResult334 = assets->json("/interface/windowconfig/crafting.config:default");
  if (!jsonResult334)
    return std::unexpected(std::move(jsonResult334.error()));
  auto fetchJsonResult335 = assets->fetchJson(baseConfig);
  if (!fetchJsonResult335)
    return std::unexpected(std::move(fetchJsonResult335.error()));
  m_settings = jsonMerge(std::move(*jsonResult334), 
               jsonMerge(std::move(*fetchJsonResult335), settings));

  auto getResult336 = m_settings.get("filter", JsonArray());
  if (!getResult336)
    return std::unexpected(std::move(getResult336.error()));
  auto jsonToStringListResult337 = jsonToStringList(std::move(*getResult336));
  if (!jsonToStringListResult337)
    return std::unexpected(std::move(jsonToStringListResult337.error()));
  m_filter = StringSet::from(std::move(*jsonToStringListResult337));
  auto getUIntResult338 = m_settings.getUInt("maxSpinCount", 1000);
  if (!getUIntResult338)
    return std::unexpected(std::move(getUIntResult338.error()));
  m_maxSpinCount = std::move(*getUIntResult338);

  GuiReader reader(context());
  reader.registerCallback("spinCount.up", [this](Widget&)  -> std::expected<void, StarException> {
      {
        auto maxCraftResult339 = maxCraft();
        if (!maxCraftResult339)
          return std::unexpected(std::move(maxCraftResult339.error()));
        if (m_count < std::move(*maxCraftResult339))
          m_count++;
        else
          m_count = 1;
      }
      auto countChangedResult340 = countChanged();
      if (!countChangedResult340)
        return std::unexpected(std::move(countChangedResult340.error()));
    
  return {};
});

  reader.registerCallback("spinCount.down", [this](Widget&)  -> std::expected<void, StarException> {
      if (m_count > 1)
        m_count--;
      else
        {
          auto maxCraftResult341 = maxCraft();
          if (!maxCraftResult341)
            return std::unexpected(std::move(maxCraftResult341.error()));
          m_count = std::max(std::move(*maxCraftResult341), 1);
        }
      auto countChangedResult342 = countChanged();
      if (!countChangedResult342)
        return std::unexpected(std::move(countChangedResult342.error()));
    
  return {};
});

  reader.registerCallback("tbSpinCount", [this](Widget&)  -> std::expected<void, StarException> { auto countTextChangedResult343 = countTextChanged();
                                                           if (!countTextChangedResult343)
                                                             return std::unexpected(std::move(countTextChangedResult343.error())); 
  return {};
});

  reader.registerCallback("close", [this](Widget&)  -> std::expected<void, StarException> { dismiss();
  return {};
});

  reader.registerCallback("btnCraft", [this](Widget&)  -> std::expected<void, StarException> { auto toggleCraftResult344 = toggleCraft();
                                                        if (!toggleCraftResult344)
                                                          return std::unexpected(std::move(toggleCraftResult344.error())); 
  return {};
});
  reader.registerCallback("btnStopCraft", [this](Widget&)  -> std::expected<void, StarException> { auto toggleCraftResult345 = toggleCraft();
                                                            if (!toggleCraftResult345)
                                                              return std::unexpected(std::move(toggleCraftResult345.error())); 
  return {};
});

  reader.registerCallback("btnFilterHaveMaterials", [this](Widget&)  -> std::expected<void, StarException> {
      auto configurationResult346 = context().root().configuration();
      if (!configurationResult346)
        return std::unexpected(std::move(configurationResult346.error()));
      auto setPathResult347 = std::move(*configurationResult346)->setPath("crafting.filterHaveMaterials", m_filterHaveMaterials->isChecked());
      if (!setPathResult347)
        return std::unexpected(std::move(setPathResult347.error()));
      auto updateAvailableRecipesResult348 = updateAvailableRecipes();
      if (!updateAvailableRecipesResult348)
        return std::unexpected(std::move(updateAvailableRecipesResult348.error()));
    
  return {};
});

  reader.registerCallback("filter", [this](Widget&)  -> std::expected<void, StarException> { auto updateAvailableRecipesResult349 = updateAvailableRecipes();
                                                      if (!updateAvailableRecipesResult349)
                                                        return std::unexpected(std::move(updateAvailableRecipesResult349.error())); 
  return {};
});

  reader.registerCallback("categories", [this](Widget&)  -> std::expected<void, StarException> { auto updateAvailableRecipesResult350 = updateAvailableRecipes();
                                                          if (!updateAvailableRecipesResult350)
                                                            return std::unexpected(std::move(updateAvailableRecipesResult350.error())); 
  return {};
});

  reader.registerCallback("rarities", [this](Widget&)  -> std::expected<void, StarException> { auto updateAvailableRecipesResult351 = updateAvailableRecipes();
                                                        if (!updateAvailableRecipesResult351)
                                                          return std::unexpected(std::move(updateAvailableRecipesResult351.error())); 
  return {};
});

  reader.registerCallback("btnUpgrade", [this](Widget&)  -> std::expected<void, StarException> { auto upgradeTableResult352 = upgradeTable();
                                                          if (!upgradeTableResult352)
                                                            return std::unexpected(std::move(upgradeTableResult352.error())); 
  return {};
});

  // this is where the GUI gets built and the buttons begin to have existence.
  // all possible callbacks must exist by this point

  auto getResult353 = m_settings.get("paneLayout");
  if (!getResult353)
    return std::unexpected(std::move(getResult353.error()));
  Json paneLayout = std::move(*getResult353);
  auto getResult354 = m_settings.get("paneLayoutOverride", {});
  if (!getResult354)
    return std::unexpected(std::move(getResult354.error()));
  paneLayout = jsonMerge(paneLayout, std::move(*getResult354));
  auto constructResult355 = reader.construct(paneLayout, *this);
  if (!constructResult355)
    return std::unexpected(std::move(constructResult355.error()));

  {
    auto fetchChildResult356 = fetchChild<ButtonWidget>("btnUpgrade");
    if (!fetchChildResult356)
      return std::unexpected(std::move(fetchChildResult356.error()));
    if (auto upgradeButton = std::move(*fetchChildResult356)) {
      upgradeButton->disable();
      auto optArrayResult357 = m_settings.optArray("upgradeMaterials");
      if (!optArrayResult357)
        return std::unexpected(std::move(optArrayResult357.error()));
      std::optional<JsonArray> recipeData = std::move(*optArrayResult357);
  
      // create a recipe out of the listed upgrade materials.
      // for ease of creating a tooltip later.
      if (recipeData) {
        m_upgradeRecipe = ItemRecipe();
        for (auto const& ingredient : *recipeData) {
          auto ingredientName = ingredient.getString("item");
          if (!ingredientName)
            return std::unexpected(std::move(ingredientName.error()));
          auto ingredientCount = ingredient.getUInt("count");
          if (!ingredientCount)
            return std::unexpected(std::move(ingredientCount.error()));
          auto ingredientDescriptor =
            ItemDescriptor::create(std::move(*ingredientName), std::move(*ingredientCount), {});
          if (!ingredientDescriptor)
            return std::unexpected(std::move(ingredientDescriptor.error()));
          m_upgradeRecipe->inputs.push_back(std::move(*ingredientDescriptor));
        }
        auto setVisibilityResult361 = upgradeButton->setVisibility(true);
        if (!setVisibilityResult361)
          return std::unexpected(std::move(setVisibilityResult361.error()));
      } else {
        auto setVisibilityResult362 = upgradeButton->setVisibility(false);
        if (!setVisibilityResult362)
          return std::unexpected(std::move(setVisibilityResult362.error()));
      }
    }
  }

  auto fetchChildResult363 = fetchChild<ListWidget>("scrollArea.itemList");
  if (!fetchChildResult363)
    return std::unexpected(std::move(fetchChildResult363.error()));
  m_guiList = std::move(*fetchChildResult363);
  auto fetchChildResult364 = fetchChild<TextBoxWidget>("tbSpinCount");
  if (!fetchChildResult364)
    return std::unexpected(std::move(fetchChildResult364.error()));
  m_textBox = std::move(*fetchChildResult364);

  auto fetchChildResult365 = fetchChild<ButtonWidget>("btnFilterHaveMaterials");
  if (!fetchChildResult365)
    return std::unexpected(std::move(fetchChildResult365.error()));
  m_filterHaveMaterials = std::move(*fetchChildResult365);
  if (m_filterHaveMaterials)
    {
      auto configurationResult366 = context().root().configuration();
      if (!configurationResult366)
        return std::unexpected(std::move(configurationResult366.error()));
      auto getPathResult367 = std::move(*configurationResult366)->getPath("crafting.filterHaveMaterials");
      if (!getPathResult367)
        return std::unexpected(std::move(getPathResult367.error()));
      auto toBoolResult368 = std::move(*getPathResult367).toBool();
      if (!toBoolResult368)
        return std::unexpected(std::move(toBoolResult368.error()));
      m_filterHaveMaterials->setChecked(std::move(*toBoolResult368));
    }

  auto fetchChildResult369 = fetchChild<ButtonWidget>("btnCraft");
  if (!fetchChildResult369)
    return std::unexpected(std::move(fetchChildResult369.error()));
  std::move(*fetchChildResult369)->disable();
  {
    auto fetchChildResult370 = fetchChild<ButtonWidget>("spinCount.up");
    if (!fetchChildResult370)
      return std::unexpected(std::move(fetchChildResult370.error()));
    if (auto spinCountUp = std::move(*fetchChildResult370))
      spinCountUp->disable();
  }
  {
    auto fetchChildResult371 = fetchChild<ButtonWidget>("spinCount.down");
    if (!fetchChildResult371)
      return std::unexpected(std::move(fetchChildResult371.error()));
    if (auto spinCountDown = std::move(*fetchChildResult371))
      spinCountDown->disable();
  }

  m_displayedRecipe = NPos;
  auto updateAvailableRecipesResult372 = updateAvailableRecipes();
  if (!updateAvailableRecipesResult372)
    return std::unexpected(std::move(updateAvailableRecipesResult372.error()));

  m_crafting = false;
  m_count = 1;
  auto countChangedResult373 = countChanged();
  if (!countChangedResult373)
    return std::unexpected(std::move(countChangedResult373.error()));

  {
    auto getBoolResult374 = m_settings.getBool("titleFromEntity", false);
    if (!getBoolResult374)
      return std::unexpected(std::move(getBoolResult374.error()));
    if (std::move(*getBoolResult374) && sourceEntityId != NullEntityId) {
      auto entity = m_worldClient->entity(sourceEntityId);
  
      if (auto container = as<ContainerEntity>(entity)) {
        {
          auto iconItemResult375 = container->iconItem();
          if (!iconItemResult375)
            return std::unexpected(std::move(iconItemResult375.error()));
          if (std::move(*iconItemResult375)) {
            auto itemDatabaseResult376 = context().root().itemDatabase();
            if (!itemDatabaseResult376)
              return std::unexpected(std::move(itemDatabaseResult376.error()));
            auto itemDatabase = std::move(*itemDatabaseResult376);
            auto iconItemResult377 = container->iconItem();
            if (!iconItemResult377)
              return std::unexpected(std::move(iconItemResult377.error()));
            auto itemSharedResult378 = itemDatabase->itemShared(std::move(*iconItemResult377));
            if (!itemSharedResult378)
              return std::unexpected(std::move(itemSharedResult378.error()));
            auto iconItem = std::move(*itemSharedResult378);
            auto createResult379 = ItemSlotWidget::create(context(), iconItem, "/interface/inventory/portrait.png");
            if (!createResult379)
              return std::unexpected(std::move(createResult379.error()));
            auto icon = std::move(*createResult379);
            String title = this->title();
            if (title.empty())
              {
                auto containerDescriptionResult380 = container->containerDescription();
                if (!containerDescriptionResult380)
                  return std::unexpected(std::move(containerDescriptionResult380.error()));
                title = std::move(*containerDescriptionResult380);
              }
            String subTitle = this->subTitle();
            if (subTitle.empty())
              {
                auto containerSubTitleResult381 = container->containerSubTitle();
                if (!containerSubTitleResult381)
                  return std::unexpected(std::move(containerSubTitleResult381.error()));
                subTitle = std::move(*containerSubTitleResult381);
              }
            icon->showRarity(false);
            auto setTitleResult382 = setTitle(icon, title, subTitle);
            if (!setTitleResult382)
              return std::unexpected(std::move(setTitleResult382.error()));
          }
        }
      }
      if (auto portaitEntity = as<PortraitEntity>(entity)) {
        auto createResult383 = PortraitWidget::create(context(), portaitEntity, PortraitMode::Bust);
        if (!createResult383)
          return std::unexpected(std::move(createResult383.error()));
        auto portrait = std::move(*createResult383);
        auto setIconModeResult384 = portrait->setIconMode();
        if (!setIconModeResult384)
          return std::unexpected(std::move(setIconModeResult384.error()));
        String title = this->title();
        if (title.empty())
          {
            auto nameResult385 = portaitEntity->name();
            if (!nameResult385)
              return std::unexpected(std::move(nameResult385.error()));
            title = std::move(*nameResult385);
          }
        String subTitle = this->subTitle();
        auto setTitleResult386 = setTitle(portrait, title, subTitle);
        if (!setTitleResult386)
          return std::unexpected(std::move(setTitleResult386.error()));
      }
    }
  }
  return {};
}

std::expected<void, StarException> CraftingPane::displayed() {
  if (auto result = Pane::displayed(); !result)
    return std::unexpected(std::move(result.error()));

  {
    auto filterWidget = fetchChild<TextBoxWidget>("filter");
    if (!filterWidget)
      return std::unexpected(std::move(filterWidget.error()));
    if (*filterWidget) {
      if (auto result = (*filterWidget)->setText(""); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = (*filterWidget)->blur(); !result)
        return std::unexpected(std::move(result.error()));
    }
  }

  if (auto result = updateAvailableRecipes(); !result)
    return std::unexpected(std::move(result.error()));
  return unlockConfiguredRecipes("initialRecipeUnlocks");
}

std::expected<void, StarException> CraftingPane::dismissed() {
  stopCrafting();
  auto dismissedResult396 = Pane::dismissed();
  if (!dismissedResult396)
    return std::unexpected(std::move(dismissedResult396.error()));
  m_itemCache.clear();
  return {};
}

std::expected<std::shared_ptr<Pane>, StarException> CraftingPane::createTooltip(Vec2I const& screenPosition) {
  for (std::size_t i = 0; i < m_guiList->numChildren(); ++i) {
    auto entry = m_guiList->itemAt(i);
    if (entry->getChildAt(screenPosition)) {
      auto recipe_lookup = m_recipesWidgetMap.get_left(entry);
      if (!recipe_lookup)
        return std::unexpected(std::move(recipe_lookup.error()));
      auto& recipe = recipe_lookup->get();
      auto setupTooltipResult398 = setupTooltip(recipe);
      if (!setupTooltipResult398)
        return std::unexpected(std::move(setupTooltipResult398.error()));
      return std::move(*setupTooltipResult398);
    }
  }

  if (std::shared_ptr<Widget> child = getChildAt(screenPosition)) {
    if (child->name() == "btnUpgrade") {
      if (m_upgradeRecipe)
        {
          auto setupTooltipResult399 = setupTooltip(*m_upgradeRecipe);
          if (!setupTooltipResult399)
            return std::unexpected(std::move(setupTooltipResult399.error()));
          return std::move(*setupTooltipResult399);
        }
    }
  }

  return {};
}

EntityId CraftingPane::sourceEntityId() const {
  return m_sourceEntityId;
}

std::expected<void, StarException> CraftingPane::unlockConfiguredRecipes(String const& setting) {
  auto recipeUnlocks = m_settings.opt(setting);
  if (!recipeUnlocks)
    return std::unexpected(std::move(recipeUnlocks.error()));
  if (!*recipeUnlocks)
    return {};

  auto itemNames = jsonToStringList(**recipeUnlocks);
  if (!itemNames)
    return std::unexpected(std::move(itemNames.error()));
  for (String const& itemName : *itemNames) {
    auto descriptor = ItemDescriptor::create(itemName);
    if (!descriptor)
      return std::unexpected(std::move(descriptor.error()));
    if (auto result = m_player->addBlueprint(std::move(*descriptor)); !result)
      return std::unexpected(std::move(result.error()));
  }
  return {};
}

std::expected<void, StarException> CraftingPane::upgradeTable() {
  if (m_sourceEntityId == NullEntityId || !m_upgradeRecipe)
    return {};

  bool canUpgrade = m_player->isAdmin();
  if (!canUpgrade) {
    auto availableItems = m_player->inventory()->availableItems();
    if (!availableItems)
      return std::unexpected(std::move(availableItems.error()));
    canUpgrade = ItemDatabase::canMakeRecipe(*m_upgradeRecipe, std::move(*availableItems),
                                             m_player->inventory()->availableCurrencies());
  }
  if (!canUpgrade)
    return {};

  if (!m_player->isAdmin()) {
    auto result = consumeIngredients(*m_upgradeRecipe, 1);
    if (!result)
      return std::unexpected(std::move(result.error()));
  }

  auto requestUpgrade = m_worldClient->sendEntityMessage(m_sourceEntityId, "requestUpgrade");
  if (!requestUpgrade)
    return std::unexpected(std::move(requestUpgrade.error()));
  if (auto result = unlockConfiguredRecipes("upgradeRecipeUnlocks"); !result)
    return std::unexpected(std::move(result.error()));

  dismiss();
  return {};
}

std::expected<std::size_t, StarException> CraftingPane::itemCount(List<std::shared_ptr<Item>> const& store, ItemDescriptor const& item) {
  auto itemDatabaseResult408 = context().root().itemDatabase();
  if (!itemDatabaseResult408)
    return std::unexpected(std::move(itemDatabaseResult408.error()));
  auto itemDb = std::move(*itemDatabaseResult408);
  return itemDb->getCountOfItem(store, item);
}

std::expected<void, StarException> CraftingPane::update(float dt) {
  // shut down if we can't reach the table anymore.
  if (m_sourceEntityId != NullEntityId) {
    auto sourceEntity = as<TileEntity>(m_worldClient->entity(m_sourceEntityId));
    {
      bool condition410 = static_cast<bool>(!sourceEntity);
      if (!condition410) {
        auto playerCanReachEntityResult409 = m_worldClient->playerCanReachEntity(m_sourceEntityId);
        if (!playerCanReachEntityResult409)
          return std::unexpected(std::move(playerCanReachEntityResult409.error()));
        condition410 = static_cast<bool>(!std::move(*playerCanReachEntityResult409));
      }
      if (condition410 || !sourceEntity->isInteractive()) {
        dismiss();
        return {};
      }
    }
  }

  // similarly if the player is dead
  if (m_player->isDead()) {
    dismiss();
    return {};
  }

  // has the selected recipe changed ?
  bool changedHighlight = (m_displayedRecipe != m_guiList->selectedItem());

  if (changedHighlight) {
    stopCrafting(); // TODO: allow viewing other recipes without interrupting crafting

    m_displayedRecipe = m_guiList->selectedItem();
    auto countTextChangedResult411 = countTextChanged();
    if (!countTextChangedResult411)
      return std::unexpected(std::move(countTextChangedResult411.error()));

    auto recipeFromSelectedWidgetResult412 = recipeFromSelectedWidget();
    if (!recipeFromSelectedWidgetResult412)
      return std::unexpected(std::move(recipeFromSelectedWidgetResult412.error()));
    auto recipe = std::move(*recipeFromSelectedWidgetResult412);

    if (recipe.isNull()) {
      auto fetchChildResult413 = fetchChild<Widget>("description");
      if (!fetchChildResult413)
        return std::unexpected(std::move(fetchChildResult413.error()));
      std::move(*fetchChildResult413)->removeAllChildren();
    } else {
      auto fetchChildResult414 = fetchChild<Widget>("description");
      if (!fetchChildResult414)
        return std::unexpected(std::move(fetchChildResult414.error()));
      auto description = std::move(*fetchChildResult414);
      description->removeAllChildren();

      auto itemDatabaseResult415 = context().root().itemDatabase();
      if (!itemDatabaseResult415)
        return std::unexpected(std::move(itemDatabaseResult415.error()));
      auto itemSharedResult416 = std::move(*itemDatabaseResult415)->itemShared(recipe.output);
      if (!itemSharedResult416)
        return std::unexpected(std::move(itemSharedResult416.error()));
      auto item = std::move(*itemSharedResult416);
      auto buildItemDescriptionResult417 = ItemTooltipBuilder::buildItemDescription(context(), description, item);
      if (!buildItemDescriptionResult417)
        return std::unexpected(std::move(buildItemDescriptionResult417.error()));
    }
  }

  // crafters gonna craft
  while (m_crafting && m_craftTimer.wrapTick(context().root().timing().timestep)) {
    auto craftCount = m_settings.getInt("craftCount", 1);
    if (!craftCount)
      return std::unexpected(std::move(craftCount.error()));
    auto crafted = craft(std::min(m_count, static_cast<int>(std::move(*craftCount))));
    if (!crafted)
      return std::unexpected(std::move(crafted.error()));
  }

  // update crafting icon, progress and buttons
  {
    auto fetchChildResult420 = fetchChild<ItemSlotWidget>("currentRecipeIcon");
    if (!fetchChildResult420)
      return std::unexpected(std::move(fetchChildResult420.error()));
    if (auto currentRecipeIcon = std::move(*fetchChildResult420)) {
      auto recipeFromSelectedWidgetResult421 = recipeFromSelectedWidget();
      if (!recipeFromSelectedWidgetResult421)
        return std::unexpected(std::move(recipeFromSelectedWidgetResult421.error()));
      auto recipe = std::move(*recipeFromSelectedWidgetResult421);
      if (recipe.isNull()) {
        currentRecipeIcon->setItem(nullptr);
      } else {
        auto single = recipe.output.singular();
        std::shared_ptr<Item> item = m_itemCache[single];
        currentRecipeIcon->setItem(item);
  
        if (m_crafting)
          currentRecipeIcon->setProgress(1.0f - m_craftTimer.percent());
        else
          currentRecipeIcon->setProgress(1.0f);
      }
    }
  }

  --m_recipeAutorefreshCooldown;

  // changed recipe or auto update time
  if (changedHighlight || (m_recipeAutorefreshCooldown <= 0)) {
    auto updateAvailableRecipesResult422 = updateAvailableRecipes();
    if (!updateAvailableRecipesResult422)
      return std::unexpected(std::move(updateAvailableRecipesResult422.error()));
    auto updateCraftButtonsResult423 = updateCraftButtons();
    if (!updateCraftButtonsResult423)
      return std::unexpected(std::move(updateCraftButtonsResult423.error()));
  }

  auto moneyLabelUpdated = setLabel("lblPlayerMoney", Star::to_string(static_cast<int>(m_player->currency("money"))));
  if (!moneyLabelUpdated)
    return std::unexpected(std::move(moneyLabelUpdated.error()));

  auto updateResult425 = Pane::update(dt);
  if (!updateResult425)
    return std::unexpected(std::move(updateResult425.error()));

return {};
}

std::expected<void, StarException> CraftingPane::updateCraftButtons() {
  auto availableItemsResult426 = m_player->inventory()->availableItems();
  if (!availableItemsResult426)
    return std::unexpected(std::move(availableItemsResult426.error()));
  auto normalizedBag = std::move(*availableItemsResult426);
  auto availableCurrencies = m_player->inventory()->availableCurrencies();

  auto recipeFromSelectedWidgetResult427 = recipeFromSelectedWidget();
  if (!recipeFromSelectedWidgetResult427)
    return std::unexpected(std::move(recipeFromSelectedWidgetResult427.error()));
  auto recipe = std::move(*recipeFromSelectedWidgetResult427);
  bool recipeAvailable = !recipe.isNull() && (m_player->isAdmin() || ItemDatabase::canMakeRecipe(recipe, normalizedBag, availableCurrencies));

  auto fetchChildResult428 = fetchChild<ButtonWidget>("btnCraft");
  if (!fetchChildResult428)
    return std::unexpected(std::move(fetchChildResult428.error()));
  std::move(*fetchChildResult428)->setEnabled(recipeAvailable);
  {
    auto fetchChildResult429 = fetchChild<ButtonWidget>("spinCount.up");
    if (!fetchChildResult429)
      return std::unexpected(std::move(fetchChildResult429.error()));
    if (auto spinCountUp = std::move(*fetchChildResult429))
      spinCountUp->setEnabled(recipeAvailable);
  }
  {
    auto fetchChildResult430 = fetchChild<ButtonWidget>("spinCount.down");
    if (!fetchChildResult430)
      return std::unexpected(std::move(fetchChildResult430.error()));
    if (auto spinCountDown = std::move(*fetchChildResult430))
      spinCountDown->setEnabled(recipeAvailable);
  }

  {
    auto fetchChildResult431 = fetchChild<ButtonWidget>("btnStopCraft");
    if (!fetchChildResult431)
      return std::unexpected(std::move(fetchChildResult431.error()));
    if (auto stopCraftButton = std::move(*fetchChildResult431)) {
      auto setVisibilityResult432 = stopCraftButton->setVisibility(m_crafting);
      if (!setVisibilityResult432)
        return std::unexpected(std::move(setVisibilityResult432.error()));
      auto fetchChildResult433 = fetchChild<ButtonWidget>("btnCraft");
      if (!fetchChildResult433)
        return std::unexpected(std::move(fetchChildResult433.error()));
      auto setVisibilityResult434 = std::move(*fetchChildResult433)->setVisibility(!m_crafting);
      if (!setVisibilityResult434)
        return std::unexpected(std::move(setVisibilityResult434.error()));
    }
  }

  {
    auto fetchChildResult435 = fetchChild<ButtonWidget>("btnUpgrade");
    if (!fetchChildResult435)
      return std::unexpected(std::move(fetchChildResult435.error()));
    if (auto upgradeButton = std::move(*fetchChildResult435)) {
      bool canUpgrade = (m_upgradeRecipe && (m_player->isAdmin() || ItemDatabase::canMakeRecipe(*m_upgradeRecipe, normalizedBag, availableCurrencies)));
      upgradeButton->setEnabled(canUpgrade);
    }
  }
  return {};
}

std::expected<void, StarException> CraftingPane::updateAvailableRecipes() {
  m_recipeAutorefreshCooldown = 30;

  StringSet categoryFilter;
  {
    auto fetchChildResult436 = fetchChild<ButtonGroupWidget>("categories");
    if (!fetchChildResult436)
      return std::unexpected(std::move(fetchChildResult436.error()));
    if (auto categoriesGroup = std::move(*fetchChildResult436)) {
      if (auto selectedCategories = categoriesGroup->checkedButton()) {
        {
          auto categoryGroups = selectedCategories->get().data().getArray("filter");
          if (!categoryGroups)
            return std::unexpected(std::move(categoryGroups.error()));
          for (auto const& group : std::move(*categoryGroups)) {
            auto categoryName = group.toString();
            if (!categoryName)
              return std::unexpected(std::move(categoryName.error()));
            categoryFilter.add(std::move(*categoryName));
          }
        }
      }
    }
  }

  HashSet<Rarity> rarityFilter;
  {
    auto fetchChildResult439 = fetchChild<ButtonGroupWidget>("rarities");
    if (!fetchChildResult439)
      return std::unexpected(std::move(fetchChildResult439.error()));
    if (auto raritiesGroup = std::move(*fetchChildResult439)) {
      if (auto selectedRarities = raritiesGroup->checkedButton()) {
        {
          auto rarityValues = selectedRarities->get().data().getArray("rarity");
          if (!rarityValues)
            return std::unexpected(std::move(rarityValues.error()));
          auto rarityNames = jsonToStringSet(std::move(*rarityValues));
          if (!rarityNames)
            return std::unexpected(std::move(rarityNames.error()));
          for (auto const& entry : std::move(*rarityNames)) {
            auto rarity = enumValue<RarityNames>(entry);
            if (!rarity)
              return std::unexpected(std::move(rarity.error()));
            rarityFilter.add(*rarity);
          }
        }
      }
    }
  }

  String filterText;
  {
    auto fetchChildResult444 = fetchChild<TextBoxWidget>("filter");
    if (!fetchChildResult444)
      return std::unexpected(std::move(fetchChildResult444.error()));
    if (auto filterWidget = std::move(*fetchChildResult444))
      filterText = filterWidget->getText();
  }

  auto determineRecipesResult445 = determineRecipes();
  if (!determineRecipesResult445)
    return std::unexpected(std::move(determineRecipesResult445.error()));
  m_recipes = std::move(*determineRecipesResult445);

  std::size_t currentOffset = 0;

  ItemRecipe selectedRecipe;
  if (m_guiList->selectedWidget())
    {
    auto selected_recipe_lookup = m_recipesWidgetMap.get_left(m_guiList->selectedWidget());
    if (!selected_recipe_lookup)
      return std::unexpected(std::move(selected_recipe_lookup.error()));
    selectedRecipe = selected_recipe_lookup->get();
    }

  auto availableItemsResult447 = m_player->inventory()->availableItems();
  if (!availableItemsResult447)
    return std::unexpected(std::move(availableItemsResult447.error()));
  HashMap<ItemDescriptor, std::uint64_t> normalizedBag = std::move(*availableItemsResult447);

  auto clearResult448 = m_guiList->clear();
  if (!clearResult448)
    return std::unexpected(std::move(clearResult448.error()));

  for (auto const& recipe : m_recipes) {
    auto widget = m_recipesWidgetMap.value_right(recipe);
    if (widget) {
      auto addItemResult449 = m_guiList->addItem(widget);
      if (!addItemResult449)
        return std::unexpected(std::move(addItemResult449.error()));
    } else {
      auto addItemResult450 = m_guiList->addItem();
      if (!addItemResult450)
        return std::unexpected(std::move(addItemResult450.error()));
      widget = std::move(*addItemResult450);
      auto addResult451 = m_recipesWidgetMap.add(recipe, widget);
      if (!addResult451)
        return std::unexpected(std::move(addResult451.error()));
    }

    auto setupWidgetResult452 = setupWidget(widget, recipe, normalizedBag);
    if (!setupWidgetResult452)
      return std::unexpected(std::move(setupWidgetResult452.error()));

    if (selectedRecipe == recipe) {
      auto setSelectedResult453 = m_guiList->setSelected(currentOffset);
      if (!setSelectedResult453)
        return std::unexpected(std::move(setSelectedResult453.error()));
    }

    currentOffset++;
  }
  return {};
}

std::expected<void, StarException> CraftingPane::setupWidget(std::shared_ptr<Widget> const& widget, ItemRecipe const& recipe, HashMap<ItemDescriptor, std::uint64_t> const& normalizedBag) {
  auto& root = context().root();

  auto single = recipe.output.singular();
  std::shared_ptr<Item> item = m_itemCache[single];
  if (!item) {
    auto itemDatabaseResult454 = root.itemDatabase();
    if (!itemDatabaseResult454)
      return std::unexpected(std::move(itemDatabaseResult454.error()));
    auto itemSharedResult455 = std::move(*itemDatabaseResult454)->itemShared(single);
    if (!itemSharedResult455)
      return std::unexpected(std::move(itemSharedResult455.error()));
    item = std::move(*itemSharedResult455);
    m_itemCache[single] = item;
  }

  bool unavailable = false;
  std::size_t price = recipe.currencyInputs.value("money", 0);

  if (!m_player->isAdmin()) {
    for (auto const& p : recipe.currencyInputs) {
      if (m_player->currency(p.first) < p.second)
        unavailable = true;
    }

    auto itemDatabaseResult456 = context().root().itemDatabase();
    if (!itemDatabaseResult456)
      return std::unexpected(std::move(itemDatabaseResult456.error()));
    auto itemDb = std::move(*itemDatabaseResult456);
    for (auto const& input : recipe.inputs) {
      if (itemDb->getCountOfItem(normalizedBag, input, recipe.matchInputParameters) < input.count())
        unavailable = true;
    }
  }

  String name = item->friendlyName();
  if (recipe.output.count() > 1)
    name = Star::format("{} (x{})", name, recipe.output.count());

  auto fetchChildResult457 = widget->fetchChild<LabelWidget>("itemName");
  if (!fetchChildResult457)
    return std::unexpected(std::move(fetchChildResult457.error()));
  auto itemName = std::move(*fetchChildResult457);
  auto fetchChildResult458 = widget->fetchChild<ImageWidget>("notcraftableoverlay");
  if (!fetchChildResult458)
    return std::unexpected(std::move(fetchChildResult458.error()));
  auto notcraftableoverlay = std::move(*fetchChildResult458);

  auto setTextResult459 = itemName->setText(name);
  if (!setTextResult459)
    return std::unexpected(std::move(setTextResult459.error()));

  if (unavailable) {
    itemName->setColor(Color::Gray);
    auto showResult460 = notcraftableoverlay->show();
    if (!showResult460)
      return std::unexpected(std::move(showResult460.error()));
  } else {
    itemName->setColor(Color::White);
    auto hideResult461 = notcraftableoverlay->hide();
    if (!hideResult461)
      return std::unexpected(std::move(hideResult461.error()));
  }

  if (price > 0) {
    auto setLabelResult462 = widget->setLabel("priceLabel", Star::to_string(price));
    if (!setLabelResult462)
      return std::unexpected(std::move(setLabelResult462.error()));
    {
      auto fetchChildResult463 = widget->fetchChild<ImageWidget>("moneyIcon");
      if (!fetchChildResult463)
        return std::unexpected(std::move(fetchChildResult463.error()));
      if (auto icon = std::move(*fetchChildResult463))
        {
          auto setVisibilityResult464 = icon->setVisibility(true);
          if (!setVisibilityResult464)
            return std::unexpected(std::move(setVisibilityResult464.error()));
          
        }
    }
  } else {
    auto setLabelResult465 = widget->setLabel("priceLabel", "");
    if (!setLabelResult465)
      return std::unexpected(std::move(setLabelResult465.error()));
    {
      auto fetchChildResult466 = widget->fetchChild<ImageWidget>("moneyIcon");
      if (!fetchChildResult466)
        return std::unexpected(std::move(fetchChildResult466.error()));
      if (auto icon = std::move(*fetchChildResult466))
        {
          auto setVisibilityResult467 = icon->setVisibility(false);
          if (!setVisibilityResult467)
            return std::unexpected(std::move(setVisibilityResult467.error()));
          
        }
    }
  }

  {
    auto fetchChildResult468 = widget->fetchChild<ImageWidget>("newIcon");
    if (!fetchChildResult468)
      return std::unexpected(std::move(fetchChildResult468.error()));
    if (auto newIndicator = std::move(*fetchChildResult468)) {
      if (m_blueprints->isNew(recipe.output.singular())) {
        auto showResult469 = newIndicator->show();
        if (!showResult469)
          return std::unexpected(std::move(showResult469.error()));
        auto setLabelResult470 = widget->setLabel("priceLabel", "");
        if (!setLabelResult470)
          return std::unexpected(std::move(setLabelResult470.error()));
        {
          auto fetchChildResult471 = widget->fetchChild<ImageWidget>("moneyIcon");
          if (!fetchChildResult471)
            return std::unexpected(std::move(fetchChildResult471.error()));
          if (auto icon = std::move(*fetchChildResult471))
            {
              auto setVisibilityResult472 = icon->setVisibility(false);
              if (!setVisibilityResult472)
                return std::unexpected(std::move(setVisibilityResult472.error()));
              
            }
        }
      } else {
        auto hideResult473 = newIndicator->hide();
        if (!hideResult473)
          return std::unexpected(std::move(hideResult473.error()));
      }
    }
  }

  auto fetchChildResult474 = widget->fetchChild<ItemSlotWidget>("itemIcon");
  if (!fetchChildResult474)
    return std::unexpected(std::move(fetchChildResult474.error()));
  std::move(*fetchChildResult474)->setItem(item);
  auto showResult475 = widget->show();
  if (!showResult475)
    return std::unexpected(std::move(showResult475.error()));

return {};
}

std::expected<std::shared_ptr<Pane>, StarException> CraftingPane::setupTooltip(ItemRecipe const& recipe) {
  auto& root = context().root();

  auto createResult476 = Pane::create(context());
  if (!createResult476)
    return std::unexpected(std::move(createResult476.error()));
  auto tooltip = std::move(*createResult476);
  GuiReader reader(context());
  auto assetsResult477 = root.assets();
  if (!assetsResult477)
    return std::unexpected(std::move(assetsResult477.error()));
  auto jsonResult478 = std::move(*assetsResult477)->json("/interface/craftingtooltip/craftingtooltip.config");
  if (!jsonResult478)
    return std::unexpected(std::move(jsonResult478.error()));
  auto constructResult479 = reader.construct(std::move(*jsonResult478), *tooltip);
  if (!constructResult479)
    return std::unexpected(std::move(constructResult479.error()));

  auto fetchChildResult480 = tooltip->fetchChild<ListWidget>("itemList");
  if (!fetchChildResult480)
    return std::unexpected(std::move(fetchChildResult480.error()));
  auto guiList = std::move(*fetchChildResult480);
  auto clearResult481 = guiList->clear();
  if (!clearResult481)
    return std::unexpected(std::move(clearResult481.error()));

  auto availableItemsResult482 = m_player->inventory()->availableItems();
  if (!availableItemsResult482)
    return std::unexpected(std::move(availableItemsResult482.error()));
  auto normalizedBag = std::move(*availableItemsResult482);

  auto itemDatabaseResult483 = root.itemDatabase();
  if (!itemDatabaseResult483)
    return std::unexpected(std::move(itemDatabaseResult483.error()));
  auto itemDb = std::move(*itemDatabaseResult483);

  auto addIngredient = [guiList](std::shared_ptr<Item> const& item, std::size_t availableCount, std::size_t requiredCount)  -> std::expected<void, StarException> {
      auto addItemResult484 = guiList->addItem();
      if (!addItemResult484)
        return std::unexpected(std::move(addItemResult484.error()));
      auto widget = std::move(*addItemResult484);
      auto fetchChildResult485 = widget->fetchChild<LabelWidget>("itemName");
      if (!fetchChildResult485)
        return std::unexpected(std::move(fetchChildResult485.error()));
      auto setTextResult486 = std::move(*fetchChildResult485)->setText(item->friendlyName());
      if (!setTextResult486)
        return std::unexpected(std::move(setTextResult486.error()));
      auto fetchChildResult487 = widget->fetchChild<LabelWidget>("count");
      if (!fetchChildResult487)
        return std::unexpected(std::move(fetchChildResult487.error()));
      auto countWidget = std::move(*fetchChildResult487);
      auto setTextResult488 = countWidget->setText(Star::format("{}/{}", availableCount, requiredCount));
      if (!setTextResult488)
        return std::unexpected(std::move(setTextResult488.error()));
      if (availableCount < requiredCount)
        countWidget->setColor(Color::Red);
      else
        countWidget->setColor(Color::Green);
      auto fetchChildResult489 = widget->fetchChild<ItemSlotWidget>("itemIcon");
      if (!fetchChildResult489)
        return std::unexpected(std::move(fetchChildResult489.error()));
      std::move(*fetchChildResult489)->setItem(item);
      auto showResult490 = widget->show();
      if (!showResult490)
        return std::unexpected(std::move(showResult490.error()));
    
  return {};
};

  auto assetsResult491 = root.assets();
  if (!assetsResult491)
    return std::unexpected(std::move(assetsResult491.error()));
  auto jsonResult492 = std::move(*assetsResult491)->json("/currencies.config");
  if (!jsonResult492)
    return std::unexpected(std::move(jsonResult492.error()));
  auto currenciesConfig = std::move(*jsonResult492);
  for (auto const& p : recipe.currencyInputs) {
    if (p.second > 0) {
      auto itemDatabaseResult493 = root.itemDatabase();
      if (!itemDatabaseResult493)
        return std::unexpected(std::move(itemDatabaseResult493.error()));
      auto getResult494 = currenciesConfig.get(p.first);
      if (!getResult494)
        return std::unexpected(std::move(getResult494.error()));
      auto getStringResult495 = std::move(*getResult494).getString("representativeItem");
      if (!getStringResult495)
        return std::unexpected(std::move(getStringResult495.error()));
      auto createResult496 = ItemDescriptor::create(std::move(*getStringResult495));
      if (!createResult496)
        return std::unexpected(std::move(createResult496.error()));
      auto itemSharedResult497 = std::move(*itemDatabaseResult493)->itemShared(std::move(*createResult496));
      if (!itemSharedResult497)
        return std::unexpected(std::move(itemSharedResult497.error()));
      auto currencyItem = std::move(*itemSharedResult497);
      addIngredient(currencyItem, m_player->currency(p.first), p.second);
    }
  }

  for (auto const& input : recipe.inputs) {
    auto itemDatabaseResult498 = root.itemDatabase();
    if (!itemDatabaseResult498)
      return std::unexpected(std::move(itemDatabaseResult498.error()));
    auto itemSharedResult499 = std::move(*itemDatabaseResult498)->itemShared(input.singular());
    if (!itemSharedResult499)
      return std::unexpected(std::move(itemSharedResult499.error()));
    auto item = std::move(*itemSharedResult499);
    std::size_t itemCount = itemDb->getCountOfItem(normalizedBag, input, recipe.matchInputParameters);
    addIngredient(item, itemCount, input.count());
  }

  auto fetchChildResult500 = tooltip->fetchChild<ImageStretchWidget>("background");
  if (!fetchChildResult500)
    return std::unexpected(std::move(fetchChildResult500.error()));
  auto background = std::move(*fetchChildResult500);
  background->setSize(background->size() + Vec2I(0, guiList->size()[1]));

  auto fetchChildResult501 = tooltip->fetchChild<LabelWidget>("title");
  if (!fetchChildResult501)
    return std::unexpected(std::move(fetchChildResult501.error()));
  auto title = std::move(*fetchChildResult501);
  title->setPosition(title->position() + Vec2I(0, guiList->size()[1]));

  tooltip->setSize(background->size());

  return tooltip;
}

std::expected<bool, StarException> CraftingPane::consumeIngredients(ItemRecipe& recipe, int count) {
  auto itemDatabaseResult502 = context().root().itemDatabase();
  if (!itemDatabaseResult502)
    return std::unexpected(std::move(itemDatabaseResult502.error()));
  auto itemDb = std::move(*itemDatabaseResult502);

  auto availableItemsResult503 = m_player->inventory()->availableItems();
  if (!availableItemsResult503)
    return std::unexpected(std::move(availableItemsResult503.error()));
  auto normalizedBag = std::move(*availableItemsResult503);
  auto availableCurrencies = m_player->inventory()->availableCurrencies();

  // make sure we still have the currencies and items avaialable
  for (auto const& p : recipe.currencyInputs) {
    std::uint64_t countRequired = p.second * count;
    if (availableCurrencies.value(p.first) < countRequired) {
      auto updateAvailableRecipesResult504 = updateAvailableRecipes();
      if (!updateAvailableRecipesResult504)
        return std::unexpected(std::move(updateAvailableRecipesResult504.error()));
      return false;
    }
  }
  for (auto input : recipe.inputs) {
    std::size_t countRequired = input.count() * count;
    if (itemDb->getCountOfItem(normalizedBag, input, recipe.matchInputParameters) < countRequired) {
      auto updateAvailableRecipesResult505 = updateAvailableRecipes();
      if (!updateAvailableRecipesResult505)
        return std::unexpected(std::move(updateAvailableRecipesResult505.error()));
      return false;
    }
  }

  // actually consume the currencies and items
  for (auto const& p : recipe.currencyInputs) {
    if (p.second > 0)
      m_player->inventory()->consumeCurrency(p.first, p.second * count);
  }
  for (auto input : recipe.inputs) {
    if (count > 0)
      {
        auto createResult506 = ItemDescriptor::create(input.name(), input.count() * count, input.parameters());
        if (!createResult506)
          return std::unexpected(std::move(createResult506.error()));
        m_player->inventory()->consumeItems(std::move(*createResult506), recipe.matchInputParameters);
      }
  }
  return true;
}

void CraftingPane::stopCrafting() {
  if (m_craftingSound)
    m_craftingSound->stop();
  m_crafting = false;
}

std::expected<void, StarException> CraftingPane::toggleCraft() {
  if (m_crafting) {
    stopCrafting();
  } else {
    auto recipeFromSelectedWidgetResult507 = recipeFromSelectedWidget();
    if (!recipeFromSelectedWidgetResult507)
      return std::unexpected(std::move(recipeFromSelectedWidgetResult507.error()));
    auto recipe = std::move(*recipeFromSelectedWidgetResult507);
    {
      bool condition509 = static_cast<bool>(recipe.duration > 0);
      if (condition509) {
        auto getBoolResult508 = m_settings.getBool("disableTimer", false);
        if (!getBoolResult508)
          return std::unexpected(std::move(getBoolResult508.error()));
        condition509 = static_cast<bool>(!std::move(*getBoolResult508));
      }
      if (condition509) {
        m_crafting = true;
        m_craftTimer = GameTimer(recipe.duration);
  
        {
          auto optStringResult510 = m_settings.optString("craftingSound");
          if (!optStringResult510)
            return std::unexpected(std::move(optStringResult510.error()));
          if (auto craftingSound = std::move(*optStringResult510)) {
            auto assetsResult511 = context().root().assets();
            if (!assetsResult511)
              return std::unexpected(std::move(assetsResult511.error()));
            auto assets = std::move(*assetsResult511);
            auto audio = assets->audio(*craftingSound);
            if (!audio)
              return std::unexpected(std::move(audio.error()));
            auto createResult512 = AudioInstance::create(**audio);
            if (!createResult512)
              return std::unexpected(std::move(createResult512.error()));
            m_craftingSound = std::move(*createResult512);
            m_craftingSound->setLoops(-1);
            auto playAudioResult513 = context().playAudio(m_craftingSound);
            if (!playAudioResult513)
              return std::unexpected(std::move(playAudioResult513.error()));
          }
        }
      } else {
        auto craftResult514 = craft(m_count);
        if (!craftResult514)
          return std::unexpected(std::move(craftResult514.error()));
      }
    }
  }
  return {};
}

std::expected<void, StarException> CraftingPane::craft(int count) {
  auto& root = context().root();

  if (m_guiList->selectedItem() != NPos) {
    auto recipeFromSelectedWidgetResult515 = recipeFromSelectedWidget();
    if (!recipeFromSelectedWidgetResult515)
      return std::unexpected(std::move(recipeFromSelectedWidgetResult515.error()));
    auto recipe = std::move(*recipeFromSelectedWidgetResult515);

    {
      bool condition517 = static_cast<bool>(!m_player->isAdmin());
      if (condition517) {
        auto consumeIngredientsResult516 = consumeIngredients(recipe, count);
        if (!consumeIngredientsResult516)
          return std::unexpected(std::move(consumeIngredientsResult516.error()));
        condition517 = static_cast<bool>(!std::move(*consumeIngredientsResult516));
      }
      if (condition517) {
        stopCrafting();
        return {};
      }
    }

    ItemDescriptor itemDescriptor = recipe.output;
    int remainingItemCount = itemDescriptor.count() * count;
    while (remainingItemCount > 0) {
      auto itemDatabaseResult518 = root.itemDatabase();
      if (!itemDatabaseResult518)
        return std::unexpected(std::move(itemDatabaseResult518.error()));
      auto itemResult519 = std::move(*itemDatabaseResult518)->item(itemDescriptor.singular().multiply(remainingItemCount));
      if (!itemResult519)
        return std::unexpected(std::move(itemResult519.error()));
      auto craftedItem = std::move(*itemResult519);
      remainingItemCount -= craftedItem->count();
      auto giveItemResult520 = m_player->giveItem(craftedItem);
      if (!giveItemResult520)
        return std::unexpected(std::move(giveItemResult520.error()));

      for (auto&& collectable : recipe.collectables) {
        auto collectable_added = m_player->addCollectable(collectable.first, collectable.second);
        if (!collectable_added)
          return std::unexpected(std::move(collectable_added.error()));
      }
    }

    m_blueprints->markAsRead(recipe.output.singular());
  }

  auto updateAvailableRecipesResult522 = updateAvailableRecipes();
  if (!updateAvailableRecipesResult522)
    return std::unexpected(std::move(updateAvailableRecipesResult522.error()));

  m_count -= count;
  if (m_count <= 0) {
    m_count = 1;
    stopCrafting();
  }
  auto countChangedResult523 = countChanged();
  if (!countChangedResult523)
    return std::unexpected(std::move(countChangedResult523.error()));

  auto updateCraftButtonsResult524 = updateCraftButtons();
  if (!updateCraftButtonsResult524)
    return std::unexpected(std::move(updateCraftButtonsResult524.error()));

return {};
}

std::expected<void, StarException> CraftingPane::countTextChanged() {
  if (m_textBox) {
    int appropriateDefaultCount = 1;
    auto replaceResult525 = m_textBox->getText().replace("x", "");
    if (!replaceResult525)
      return std::unexpected(std::move(replaceResult525.error()));
    auto countText = std::move(*replaceResult525);
    if (countText.empty()) {
      m_count = appropriateDefaultCount;
    } else {
      auto parsed = lexical_cast<int>(countText);
      if (!parsed) {
        if (!parsed.error().is<BadLexicalCast>())
          return std::unexpected(std::move(parsed.error()));
        m_count = appropriateDefaultCount;
      } else {
        auto maxCraftResult526 = maxCraft();
        if (!maxCraftResult526)
          return std::unexpected(std::move(maxCraftResult526.error()));
        m_count = clamp<int>(*parsed, appropriateDefaultCount, std::move(*maxCraftResult526));
      }
      auto countChangedResult527 = countChanged();
      if (!countChangedResult527)
        return std::unexpected(std::move(countChangedResult527.error()));
    }
  } else {
    m_count = 1;
  }
  return {};
}

std::expected<void, StarException> CraftingPane::countChanged() {
  if (m_textBox)
    {
      auto setTextResult528 = m_textBox->setText(Star::format("x{}", m_count), false);
      if (!setTextResult528)
        return std::unexpected(std::move(setTextResult528.error()));
      
    }

return {};
}

std::expected<List<ItemRecipe>, StarException> CraftingPane::determineRecipes() {
  HashSet<ItemRecipe> recipes;
  auto itemDatabaseResult529 = context().root().itemDatabase();
  if (!itemDatabaseResult529)
    return std::unexpected(std::move(itemDatabaseResult529.error()));
  auto itemDb = std::move(*itemDatabaseResult529);

  StringSet categoryFilter;
  {
    auto fetchChildResult530 = fetchChild<ButtonGroupWidget>("categories");
    if (!fetchChildResult530)
      return std::unexpected(std::move(fetchChildResult530.error()));
    if (auto categoriesGroup = std::move(*fetchChildResult530)) {
      if (auto selectedCategories = categoriesGroup->checkedButton()) {
        {
          auto categoryGroups = selectedCategories->get().data().getArray("filter");
          if (!categoryGroups)
            return std::unexpected(std::move(categoryGroups.error()));
          for (auto const& group : std::move(*categoryGroups)) {
            auto categoryName = group.toString();
            if (!categoryName)
              return std::unexpected(std::move(categoryName.error()));
            categoryFilter.add(std::move(*categoryName));
          }
        }
      }
    }
  }

  HashSet<Rarity> rarityFilter;
  {
    auto fetchChildResult533 = fetchChild<ButtonGroupWidget>("rarities");
    if (!fetchChildResult533)
      return std::unexpected(std::move(fetchChildResult533.error()));
    if (auto raritiesGroup = std::move(*fetchChildResult533)) {
      if (auto selectedRarities = raritiesGroup->checkedButton()) {
        {
          auto rarityValues = selectedRarities->get().data().getArray("rarity");
          if (!rarityValues)
            return std::unexpected(std::move(rarityValues.error()));
          auto rarityNames = jsonToStringSet(std::move(*rarityValues));
          if (!rarityNames)
            return std::unexpected(std::move(rarityNames.error()));
          for (auto const& entry : std::move(*rarityNames)) {
            auto rarity = enumValue<RarityNames>(entry);
            if (!rarity)
              return std::unexpected(std::move(rarity.error()));
            rarityFilter.add(*rarity);
          }
        }
      }
    }
  }

  String filterText;
  {
    auto fetchChildResult538 = fetchChild<TextBoxWidget>("filter");
    if (!fetchChildResult538)
      return std::unexpected(std::move(fetchChildResult538.error()));
    if (auto filterWidget = std::move(*fetchChildResult538))
      filterText = filterWidget->getText();
  }

  bool filterHaveMaterials = false;
  if (m_filterHaveMaterials)
    filterHaveMaterials = m_filterHaveMaterials->isChecked();

  {
    auto getBoolResult539 = m_settings.getBool("printer", false);
    if (!getBoolResult539)
      return std::unexpected(std::move(getBoolResult539.error()));
    if (std::move(*getBoolResult539)) {
      auto objectDatabaseResult540 = context().root().objectDatabase();
      if (!objectDatabaseResult540)
        return std::unexpected(std::move(objectDatabaseResult540.error()));
      auto objectDatabase = std::move(*objectDatabaseResult540);
  
      StringList itemList;
      if (m_player->isAdmin())
        itemList = objectDatabase->allObjects();
      else
        itemList = StringList::from(m_player->log()->scannedObjects());
  
      StringList printableItems;
      for (auto const& itemName : itemList) {
        if (objectDatabase->isObject(itemName)) {
          {
            auto getConfigResult541 = objectDatabase->getConfig(itemName);
            if (!getConfigResult541)
              return std::unexpected(std::move(getConfigResult541.error()));
            if (auto objectConfig = std::move(*getConfigResult541)) {
              if (objectConfig->printable && itemDb->hasItem(itemName))
                printableItems.push_back(itemName);
            }
          }
        }
      }
      itemList = std::move(printableItems);
  
      auto getFloatResult542 = m_settings.getFloat("printTime", 0);
      if (!getFloatResult542)
        return std::unexpected(std::move(getFloatResult542.error()));
      float printTime = std::move(*getFloatResult542);
      auto getFloatResult543 = m_settings.getFloat("printCostFactor", 1.0);
      if (!getFloatResult543)
        return std::unexpected(std::move(getFloatResult543.error()));
      float printFactor = std::move(*getFloatResult543);
      for (auto& itemName : itemList) {
        ItemRecipe recipe;
        auto createResult544 = ItemDescriptor::create(itemName, 1);
        if (!createResult544)
          return std::unexpected(std::move(createResult544.error()));
        recipe.output = std::move(*createResult544);
        auto itemSharedResult545 = itemDb->itemShared(recipe.output);
        if (!itemSharedResult545)
          return std::unexpected(std::move(itemSharedResult545.error()));
        auto recipeItem = std::move(*itemSharedResult545);
        int itemPrice = static_cast<int>(recipeItem->price() * printFactor);
        recipe.currencyInputs["money"] = itemPrice;
        recipe.outputRarity = recipeItem->rarity();
        recipe.duration = printTime;
        auto guiFilterStringResult546 = ItemDatabase::guiFilterString(recipeItem);
        if (!guiFilterStringResult546)
          return std::unexpected(std::move(guiFilterStringResult546.error()));
        recipe.guiFilterString = std::move(*guiFilterStringResult546);
        auto getConfigResult547 = objectDatabase->getConfig(itemName);
        if (!getConfigResult547)
          return std::unexpected(std::move(getConfigResult547.error()));
        recipe.groups = StringSet{std::move(*getConfigResult547)->category};
        recipes.add(recipe);
      }
    } else {
             auto containsResult548 = m_settings.contains("recipes");
             if (!containsResult548)
               return std::unexpected(std::move(containsResult548.error()));
             if (std::move(*containsResult548)) {
        {
          auto getArrayResult549 = m_settings.getArray("recipes");
          if (!getArrayResult549)
            return std::unexpected(std::move(getArrayResult549.error()));
          for (auto& entry : std::move(*getArrayResult549)) {
            if (entry.type() == Json::Type::String)
              {
                auto toStringResult550 = entry.toString();
                if (!toStringResult550)
                  return std::unexpected(std::move(toStringResult550.error()));
                recipes.insert_range(itemDb->recipesForOutputItem(std::move(*toStringResult550)));
              }
            else
              {
                auto parseRecipeResult551 = itemDb->parseRecipe(entry);
                if (!parseRecipeResult551)
                  return std::unexpected(std::move(parseRecipeResult551.error()));
                recipes.add(std::move(*parseRecipeResult551));
              }
          }
        }
    
        if (filterHaveMaterials)
          {
          auto availableItems = m_player->inventory()->availableItems();
          if (!availableItems)
            return std::unexpected(std::move(availableItems.error()));
          recipes.insert_range(itemDb->recipesFromSubset(std::move(*availableItems),
                                                   m_player->inventory()->availableCurrencies(),
                                                   std::exchange(recipes, {}), m_filter));
          }
      } else {
        if (filterHaveMaterials)
          {
            auto availableItemsResult553 = m_player->inventory()->availableItems();
            if (!availableItemsResult553)
              return std::unexpected(std::move(availableItemsResult553.error()));
            recipes.insert_range(itemDb->recipesFromBagContents(std::move(*availableItemsResult553), m_player->inventory()->availableCurrencies(), m_filter));
          }
        else
          recipes.insert_range(itemDb->allRecipes(m_filter));
      }
           }
  }

  {
    bool condition555 = static_cast<bool>(!m_player->isAdmin());
    if (condition555) {
      auto getBoolResult554 = m_settings.getBool("requiresBlueprint", true);
      if (!getBoolResult554)
        return std::unexpected(std::move(getBoolResult554.error()));
      condition555 = static_cast<bool>(std::move(*getBoolResult554));
    }
    if (condition555) {
      auto tempRecipes = std::exchange(recipes, {});
      for (auto const& recipe : tempRecipes) {
        if (m_blueprints->isKnown(recipe.output))
          recipes.add(recipe);
      }
    }
  }

  if (!categoryFilter.empty()) {
    auto temprecipes = std::exchange(recipes, {});
    for (auto const& recipe : temprecipes) {
      if (recipe.groups.has_intersection(categoryFilter))
        recipes.add(recipe);
    }
  }

  if (!rarityFilter.empty()) {
    auto temprecipes = std::exchange(recipes, {});
    for (auto const& recipe : temprecipes) {
      if (recipe.output) {
        if (rarityFilter.contains(recipe.outputRarity))
          recipes.add(recipe);
      }
    }
  }

  if (!filterText.empty()) {
    auto toLowerResult556 = filterText.to_lower();
    if (!toLowerResult556)
      return std::unexpected(std::move(toLowerResult556.error()));
    auto splitAnyResult557 = std::move(*toLowerResult556).split_any(" ,.?*\\+/|\t");
    if (!splitAnyResult557)
      return std::unexpected(std::move(splitAnyResult557.error()));
    auto bits = std::move(*splitAnyResult557);
    auto temprecipes = std::exchange(recipes, {});
    for (auto const& recipe : temprecipes) {
      if (recipe.output) {
        bool match = true;
        auto guiFilterString = recipe.guiFilterString;
        for (auto const& bit : bits) {
          auto containsResult558 = guiFilterString.contains(bit);
          if (!containsResult558)
            return std::unexpected(std::move(containsResult558.error()));
          match &= std::move(*containsResult558);
          if (!match)
            break;
        }
        if (match)
          recipes.add(recipe);
      }
    }
  }

  List<ItemRecipe> sortedRecipes = recipes.values();
  auto itemDatabaseResult559 = context().root().itemDatabase();
  if (!itemDatabaseResult559)
    return std::unexpected(std::move(itemDatabaseResult559.error()));
  auto itemDatabase = std::move(*itemDatabaseResult559);
  auto sortByComputedValueResult563 = sortByComputedValue(sortedRecipes, [itemDatabase](ItemRecipe const& recipe) -> std::expected<std::tuple<String, String>, StarException> {
      auto itemFriendlyNameResult560 = itemDatabase->itemFriendlyName(recipe.output.name());
      if (!itemFriendlyNameResult560)
        return std::unexpected(std::move(itemFriendlyNameResult560.error()));
      auto trimResult561 = std::move(*itemFriendlyNameResult560).trim();
      if (!trimResult561)
        return std::unexpected(std::move(trimResult561.error()));
      auto toLowerResult562 = std::move(*trimResult561).to_lower();
      if (!toLowerResult562)
        return std::unexpected(std::move(toLowerResult562.error()));
      return std::make_tuple(std::move(*toLowerResult562), recipe.output.name());
    });
  if (!sortByComputedValueResult563)
    return std::unexpected(std::move(sortByComputedValueResult563.error()));

  return sortedRecipes;
}

std::expected<int, StarException> CraftingPane::maxCraft() {
  if (m_player->isAdmin())
    return m_maxSpinCount;
  auto itemDatabaseResult564 = context().root().itemDatabase();
  if (!itemDatabaseResult564)
    return std::unexpected(std::move(itemDatabaseResult564.error()));
  auto itemDb = std::move(*itemDatabaseResult564);
  int res = 0;
  if (m_guiList->selectedItem() != NPos && m_guiList->selectedItem() < m_recipes.size()) {
    auto availableItemsResult565 = m_player->inventory()->availableItems();
    if (!availableItemsResult565)
      return std::unexpected(std::move(availableItemsResult565.error()));
    HashMap<ItemDescriptor, std::uint64_t> normalizedBag = std::move(*availableItemsResult565);
    auto recipeFromSelectedWidgetResult566 = recipeFromSelectedWidget();
    if (!recipeFromSelectedWidgetResult566)
      return std::unexpected(std::move(recipeFromSelectedWidgetResult566.error()));
    auto selectedRecipe = std::move(*recipeFromSelectedWidgetResult566);
    res = itemDb->maxCraftableInBag(normalizedBag, m_player->inventory()->availableCurrencies(), selectedRecipe);
    res = std::min(res, m_maxSpinCount);
  }
  return res;
}

std::expected<ItemRecipe, StarException> CraftingPane::recipeFromSelectedWidget() const {
  auto pane = m_guiList->selectedWidget();
  if (pane && m_recipesWidgetMap.contains_right(pane)) {
    auto recipe_lookup = m_recipesWidgetMap.get_left(pane);
    if (!recipe_lookup)
      return std::unexpected(std::move(recipe_lookup.error()));
    return recipe_lookup->get();
  }
  return ItemRecipe();
}

}
