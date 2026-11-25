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

CraftingPane::CraftingPane(ConstructionKey, GuiContext& gui_context) : Pane(gui_context) {}

std::expected<std::shared_ptr<CraftingPane>, StarException> CraftingPane::create(GuiContext& gui_context, std::shared_ptr<WorldClient> world_client, std::shared_ptr<Player> player, Json const& settings, EntityId source_entity_id) {
  auto pane = std::make_shared<CraftingPane>(ConstructionKey{}, gui_context);
  auto base = pane->initializePane();
  if (!base)
    return std::unexpected(std::move(base.error()));
  auto initialized = pane->initialize(std::move(world_client), std::move(player), settings, source_entity_id);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return pane;
}

std::expected<void, StarException> CraftingPane::initialize(std::shared_ptr<WorldClient> world_client, std::shared_ptr<Player> player, Json const& settings, EntityId source_entity_id) {
  m_world_client = std::move(world_client);
  m_player = std::move(player);
  m_blueprints = m_player->blueprints();
  m_source_entity_id = source_entity_id;

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
  auto fetchJsonResult335 = assets->fetch_json(baseConfig);
  if (!fetchJsonResult335)
    return std::unexpected(std::move(fetchJsonResult335.error()));
  m_settings = json_merge(std::move(*jsonResult334), 
               json_merge(std::move(*fetchJsonResult335), settings));

  auto getResult336 = m_settings.get("filter", JsonArray());
  if (!getResult336)
    return std::unexpected(std::move(getResult336.error()));
  auto jsonToStringListResult337 = json_to_string_list(std::move(*getResult336));
  if (!jsonToStringListResult337)
    return std::unexpected(std::move(jsonToStringListResult337.error()));
  m_filter = StringSet::from(std::move(*jsonToStringListResult337));
  auto max_spin_count = m_settings.get_uint("maxSpinCount", 1000);
  if (!max_spin_count)
    return std::unexpected(std::move(max_spin_count.error()));
  m_max_spin_count = *max_spin_count;

  GuiReader reader(context());
  reader.registerCallback("spinCount.up", [this](Widget&) -> std::expected<void, StarException> {
    auto limit = max_craft();
    if (!limit)
      return std::unexpected(std::move(limit.error()));
    m_count = m_count < *limit ? m_count + 1 : 1;
    return count_changed();
  });

  reader.registerCallback("spinCount.down", [this](Widget&) -> std::expected<void, StarException> {
    if (m_count > 1) {
      --m_count;
    } else {
      auto limit = max_craft();
      if (!limit)
        return std::unexpected(std::move(limit.error()));
      m_count = std::max(*limit, 1);
    }
    return count_changed();
  });

  reader.registerCallback("tbSpinCount", [this](Widget&) { return count_text_changed(); });

  reader.registerCallback("close", [this](Widget&) -> std::expected<void, StarException> {
    dismiss();
    return {};
  });

  reader.registerCallback("btnCraft", [this](Widget&) { return toggle_craft(); });
  reader.registerCallback("btnStopCraft", [this](Widget&) { return toggle_craft(); });

  reader.registerCallback("btnFilterHaveMaterials", [this](Widget&) -> std::expected<void, StarException> {
    auto configuration = context().root().configuration();
    if (!configuration)
      return std::unexpected(std::move(configuration.error()));
    auto saved = (*configuration)->set_path("crafting.filterHaveMaterials", m_filter_have_materials->isChecked());
    if (!saved)
      return std::unexpected(std::move(saved.error()));
    return refresh_recipes();
  });

  reader.registerCallback("filter", [this](Widget&) { return refresh_recipes(); });
  reader.registerCallback("categories", [this](Widget&) { return refresh_recipes(); });
  reader.registerCallback("rarities", [this](Widget&) { return refresh_recipes(); });
  reader.registerCallback("btnUpgrade", [this](Widget&) { return upgrade_table(); });

  // this is where the GUI gets built and the buttons begin to have existence.
  // all possible callbacks must exist by this point

  auto getResult353 = m_settings.get("paneLayout");
  if (!getResult353)
    return std::unexpected(std::move(getResult353.error()));
  Json paneLayout = std::move(*getResult353);
  auto getResult354 = m_settings.get("paneLayoutOverride", {});
  if (!getResult354)
    return std::unexpected(std::move(getResult354.error()));
  paneLayout = json_merge(paneLayout, std::move(*getResult354));
  auto constructResult355 = reader.construct(paneLayout, *this);
  if (!constructResult355)
    return std::unexpected(std::move(constructResult355.error()));

  {
    auto fetchChildResult356 = fetchChild<ButtonWidget>("btnUpgrade");
    if (!fetchChildResult356)
      return std::unexpected(std::move(fetchChildResult356.error()));
    if (auto upgradeButton = std::move(*fetchChildResult356)) {
      upgradeButton->disable();
      auto optArrayResult357 = m_settings.opt_array("upgradeMaterials");
      if (!optArrayResult357)
        return std::unexpected(std::move(optArrayResult357.error()));
      std::optional<JsonArray> recipeData = std::move(*optArrayResult357);
  
      // create a recipe out of the listed upgrade materials.
      // for ease of creating a tooltip later.
      if (recipeData) {
        m_upgrade_recipe.emplace();
        for (auto const& ingredient : *recipeData) {
          auto ingredientName = ingredient.get_string("item");
          if (!ingredientName)
            return std::unexpected(std::move(ingredientName.error()));
          auto ingredientCount = ingredient.get_uint("count");
          if (!ingredientCount)
            return std::unexpected(std::move(ingredientCount.error()));
          auto ingredientDescriptor =
            ItemDescriptor::create(std::move(*ingredientName), std::move(*ingredientCount), {});
          if (!ingredientDescriptor)
            return std::unexpected(std::move(ingredientDescriptor.error()));
          m_upgrade_recipe->inputs.push_back(std::move(*ingredientDescriptor));
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
  m_gui_list = std::move(*fetchChildResult363);
  auto fetchChildResult364 = fetchChild<TextBoxWidget>("tbSpinCount");
  if (!fetchChildResult364)
    return std::unexpected(std::move(fetchChildResult364.error()));
  m_text_box = std::move(*fetchChildResult364);

  auto fetchChildResult365 = fetchChild<ButtonWidget>("btnFilterHaveMaterials");
  if (!fetchChildResult365)
    return std::unexpected(std::move(fetchChildResult365.error()));
  m_filter_have_materials = std::move(*fetchChildResult365);
  if (m_filter_have_materials)
    {
      auto configuration = context().root().configuration();
      if (!configuration)
        return std::unexpected(std::move(configuration.error()));
      auto filter = (*configuration)->get_path("crafting.filterHaveMaterials");
      if (!filter)
        return std::unexpected(std::move(filter.error()));
      auto checked = filter->to_bool();
      if (!checked)
        return std::unexpected(std::move(checked.error()));
      m_filter_have_materials->setChecked(*checked);
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

  m_displayed_recipe = npos;
  if (auto result = refresh_recipes(); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = count_changed(); !result)
    return std::unexpected(std::move(result.error()));

  {
    auto title_from_entity = m_settings.get_bool("titleFromEntity", false);
    if (!title_from_entity)
      return std::unexpected(std::move(title_from_entity.error()));
    if (*title_from_entity && source_entity_id != null_entity_id) {
      auto entity = m_world_client->entity(source_entity_id);
  
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
            auto item_shared_result378 = itemDatabase->item_shared(std::move(*iconItemResult377));
            if (!item_shared_result378)
              return std::unexpected(std::move(item_shared_result378.error()));
            auto iconItem = std::move(*item_shared_result378);
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
    auto filter_widget = fetchChild<TextBoxWidget>("filter");
    if (!filter_widget)
      return std::unexpected(std::move(filter_widget.error()));
    if (*filter_widget) {
      if (auto result = (*filter_widget)->setText("", false); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = (*filter_widget)->blur(); !result)
        return std::unexpected(std::move(result.error()));
    }
  }

  if (auto result = refresh_recipes(); !result)
    return std::unexpected(std::move(result.error()));
  return unlock_configured_recipes("initialRecipeUnlocks");
}

std::expected<void, StarException> CraftingPane::dismissed() {
  stop_crafting();
  if (auto result = Pane::dismissed(); !result)
    return std::unexpected(std::move(result.error()));
  m_item_cache.clear();
  return {};
}

std::expected<std::shared_ptr<Pane>, StarException> CraftingPane::createTooltip(Vec2I const& screen_position) {
  for (std::size_t i = 0; i < m_gui_list->numChildren(); ++i) {
    auto entry = m_gui_list->itemAt(i);
    if (entry->getChildAt(screen_position)) {
      auto recipe = m_recipe_widgets.get_left(entry);
      if (!recipe)
        return std::unexpected(std::move(recipe.error()));
      return setup_tooltip(recipe->get());
    }
  }

  if (auto child = getChildAt(screen_position); child && child->name() == "btnUpgrade" && m_upgrade_recipe)
    return setup_tooltip(*m_upgrade_recipe);

  return {};
}

EntityId CraftingPane::source_entity_id() const {
  return m_source_entity_id;
}

std::expected<void, StarException> CraftingPane::unlock_configured_recipes(String const& setting) {
  auto recipe_unlocks = m_settings.opt(setting);
  if (!recipe_unlocks)
    return std::unexpected(std::move(recipe_unlocks.error()));
  if (!*recipe_unlocks)
    return {};

  auto item_names = json_to_string_list(**recipe_unlocks);
  if (!item_names)
    return std::unexpected(std::move(item_names.error()));
  for (String const& item_name : *item_names) {
    auto descriptor = ItemDescriptor::create(item_name);
    if (!descriptor)
      return std::unexpected(std::move(descriptor.error()));
    if (auto result = m_player->addBlueprint(std::move(*descriptor)); !result)
      return std::unexpected(std::move(result.error()));
  }
  return {};
}

std::expected<void, StarException> CraftingPane::upgrade_table() {
  if (m_source_entity_id == null_entity_id || !m_upgrade_recipe)
    return {};

  if (!m_player->isAdmin()) {
    auto consumed = m_player->inventory()->consume_recipe(*m_upgrade_recipe, 1);
    if (!consumed)
      return std::unexpected(std::move(consumed.error()));
    if (!*consumed)
      return refresh_recipes();
  }

  auto request_upgrade = m_world_client->send_entity_message(m_source_entity_id, "requestUpgrade");
  if (!request_upgrade)
    return std::unexpected(std::move(request_upgrade.error()));
  if (auto result = unlock_configured_recipes("upgradeRecipeUnlocks"); !result)
    return std::unexpected(std::move(result.error()));

  dismiss();
  return {};
}

std::expected<void, StarException> CraftingPane::update(float dt) {
  // Shut down if we can't reach the table anymore.
  if (m_source_entity_id != null_entity_id) {
    auto source_entity = as<TileEntity>(m_world_client->entity(m_source_entity_id));
    bool can_reach = false;
    if (source_entity) {
      auto reachable = m_world_client->playerCanReachEntity(m_source_entity_id);
      if (!reachable)
        return std::unexpected(std::move(reachable.error()));
      can_reach = *reachable && source_entity->isInteractive();
    }
    if (!can_reach) {
      dismiss();
      return {};
    }
  }

  // similarly if the player is dead
  if (m_player->isDead()) {
    dismiss();
    return {};
  }

  bool selection_changed = m_displayed_recipe != m_gui_list->selectedItem();
  if (selection_changed) {
    stop_crafting();
    m_displayed_recipe = m_gui_list->selectedItem();

    auto recipe = selected_recipe();
    if (!recipe)
      return std::unexpected(std::move(recipe.error()));
    auto description = fetchChild<Widget>("description");
    if (!description)
      return std::unexpected(std::move(description.error()));
    (*description)->removeAllChildren();
    if (*recipe && !(**recipe).get().is_null()) {
      auto database = context().root().itemDatabase();
      if (!database)
        return std::unexpected(std::move(database.error()));
      auto item = (*database)->item_shared((**recipe).get().output);
      if (!item)
        return std::unexpected(std::move(item.error()));
      if (auto result = ItemTooltipBuilder::buildItemDescription(context(), *description, *item); !result)
        return std::unexpected(std::move(result.error()));
    }
  }

  // Like other periodic consumers, finish at most one batch per update.
  if (m_crafting && m_craft_timer.wrap_tick(dt)) {
    auto crafted = craft(std::min(m_count, m_batch_size));
    if (!crafted)
      return std::unexpected(std::move(crafted.error()));
  }

  auto icon_widget = fetchChild<ItemSlotWidget>("currentRecipeIcon");
  if (!icon_widget)
    return std::unexpected(std::move(icon_widget.error()));
  if (auto const& icon = *icon_widget) {
    auto recipe = selected_recipe();
    if (!recipe)
      return std::unexpected(std::move(recipe.error()));
    if (!*recipe || (**recipe).get().is_null()) {
      icon->setItem({});
    } else {
      icon->setItem(m_item_cache.value((**recipe).get().output.singular()));
      icon->setProgress(m_crafting ? 1.0f - m_craft_timer.remaining_fraction() : 1.0f);
    }
  }

  --m_recipe_refresh_cooldown;
  if (selection_changed || m_recipe_refresh_cooldown <= 0) {
    if (auto result = refresh_recipes(selection_changed); !result)
      return std::unexpected(std::move(result.error()));
  }

  auto money_label = setLabel("lblPlayerMoney", Star::to_string(static_cast<int>(m_player->currency("money"))));
  if (!money_label)
    return std::unexpected(std::move(money_label.error()));
  return Pane::update(dt);
}

std::expected<CraftingPane::RecipeInventory, StarException> CraftingPane::capture_recipe_inventory() const {
  auto const& inventory = m_player->inventory();
  auto stock = inventory->available_items();
  if (!stock)
    return std::unexpected(std::move(stock.error()));
  return RecipeInventory{std::move(*stock), inventory->available_currencies(), m_player->isAdmin()};
}

std::expected<void, StarException> CraftingPane::update_craft_buttons(RecipeInventory const& inventory, bool selected_available) {
  auto craft_button = fetchChild<ButtonWidget>("btnCraft");
  if (!craft_button)
    return std::unexpected(std::move(craft_button.error()));
  (*craft_button)->setEnabled(selected_available);

  static std::array<String, 2> const spin_names{"spinCount.up", "spinCount.down"};
  for (auto const& name : spin_names) {
    auto spin_button = fetchChild<ButtonWidget>(name);
    if (!spin_button)
      return std::unexpected(std::move(spin_button.error()));
    if (*spin_button)
      (*spin_button)->setEnabled(selected_available);
  }

  auto stop_button = fetchChild<ButtonWidget>("btnStopCraft");
  if (!stop_button)
    return std::unexpected(std::move(stop_button.error()));
  if (*stop_button) {
    if (auto result = (*stop_button)->setVisibility(m_crafting); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = (*craft_button)->setVisibility(!m_crafting); !result)
      return std::unexpected(std::move(result.error()));
  }

  auto upgrade_button = fetchChild<ButtonWidget>("btnUpgrade");
  if (!upgrade_button)
    return std::unexpected(std::move(upgrade_button.error()));
  if (*upgrade_button) {
    bool can_upgrade = m_upgrade_recipe &&
        (inventory.admin || ItemDatabase::can_make_recipe(*m_upgrade_recipe, inventory.stock, inventory.balances));
    (*upgrade_button)->setEnabled(can_upgrade);
  }
  return {};
}

std::expected<void, StarException> CraftingPane::refresh_recipes(bool selection_changed) {
  // Rebuilding and restoring the list can synchronously invoke its callback.
  // A refresh requested by that callback is already represented by this pass.
  if (m_refreshing_recipes)
    return {};
  m_refreshing_recipes = true;
  auto reset_refresh = make_scope_exit([this] { m_refreshing_recipes = false; });

  m_recipe_refresh_cooldown = 30;

  // Keep one owned snapshot for this synchronous refresh; UI callbacks and
  // item builders cannot make individual rows observe a newer inventory.
  auto inventory = capture_recipe_inventory();
  if (!inventory)
    return std::unexpected(std::move(inventory.error()));
  if (selection_changed) {
    if (auto result = count_text_changed(std::cref(*inventory)); !result)
      return std::unexpected(std::move(result.error()));
  }

  auto listing = determine_recipes(*inventory);
  if (!listing)
    return std::unexpected(std::move(listing.error()));

  auto previous_selection = m_gui_list->selectedWidget();
  if (auto result = m_gui_list->clear(); !result)
    return std::unexpected(std::move(result.error()));

  std::size_t restored_selection = npos;
  bool selected_available = false;
  for (std::size_t row = 0; row < listing->recipes.size(); ++row) {
    auto const& recipe = listing->recipes[row];
    auto widget = m_recipe_widgets.value_right(recipe);
    if (widget) {
      if (auto added = m_gui_list->addItem(widget); !added)
        return std::unexpected(std::move(added.error()));
    } else {
      auto added = m_gui_list->addItem();
      if (!added)
        return std::unexpected(std::move(added.error()));
      widget = std::move(*added);
      if (auto indexed = m_recipe_widgets.add(recipe, widget); !indexed)
        return std::unexpected(std::move(indexed.error()));
    }

    bool available = listing->all_craftable || inventory->admin ||
        ItemDatabase::can_make_recipe(recipe, inventory->stock, inventory->balances);
    if (auto result = setup_widget(widget, recipe, available); !result)
      return std::unexpected(std::move(result.error()));
    if (widget == previous_selection) {
      restored_selection = row;
      selected_available = available && !recipe.is_null();
    }
  }

  // Restore only after every row exists; setSelected can invoke a callback.
  if (restored_selection != npos) {
    if (auto result = m_gui_list->setSelected(restored_selection); !result)
      return std::unexpected(std::move(result.error()));
  }
  if (m_gui_list->selectedItem() != restored_selection) {
    auto recipe = selected_recipe();
    if (!recipe)
      return std::unexpected(std::move(recipe.error()));
    selected_available = *recipe && !(**recipe).get().is_null() &&
        (listing->all_craftable || inventory->admin ||
         ItemDatabase::can_make_recipe((**recipe).get(), inventory->stock, inventory->balances));
  }
  return update_craft_buttons(*inventory, selected_available);
}

std::expected<void, StarException> CraftingPane::setup_widget(std::shared_ptr<Widget> const& widget, ItemRecipe const& recipe, bool available) {
  auto single = recipe.output.singular();
  auto item = m_item_cache.value(single);
  if (!item) {
    auto database = context().root().itemDatabase();
    if (!database)
      return std::unexpected(std::move(database.error()));
    auto shared = (*database)->item_shared(single);
    if (!shared)
      return std::unexpected(std::move(shared.error()));
    item = std::move(*shared);
    m_item_cache[single] = item;
  }

  String name = item->friendly_name();
  if (recipe.output.count() > 1)
    name = Star::format("{} (x{})", name, recipe.output.count());

  auto item_name = widget->fetchChild<LabelWidget>("itemName");
  if (!item_name)
    return std::unexpected(std::move(item_name.error()));
  auto unavailable_overlay = widget->fetchChild<ImageWidget>("notcraftableoverlay");
  if (!unavailable_overlay)
    return std::unexpected(std::move(unavailable_overlay.error()));
  if (auto result = (*item_name)->setText(name); !result)
    return std::unexpected(std::move(result.error()));
  (*item_name)->setColor(available ? Color::palette_white : Color::palette_gray);
  if (auto result = (*unavailable_overlay)->setVisibility(!available); !result)
    return std::unexpected(std::move(result.error()));

  auto price = recipe.currency_inputs.value("money", 0);
  auto new_indicator = widget->fetchChild<ImageWidget>("newIcon");
  if (!new_indicator)
    return std::unexpected(std::move(new_indicator.error()));
  if (*new_indicator) {
    bool is_new = m_blueprints->isNew(single);
    if (auto result = (*new_indicator)->setVisibility(is_new); !result)
      return std::unexpected(std::move(result.error()));
    if (is_new)
      price = 0;
  }

  if (auto result = widget->setLabel("priceLabel", price > 0 ? Star::to_string(price) : String()); !result)
    return std::unexpected(std::move(result.error()));
  auto money_icon = widget->fetchChild<ImageWidget>("moneyIcon");
  if (!money_icon)
    return std::unexpected(std::move(money_icon.error()));
  if (*money_icon) {
    if (auto result = (*money_icon)->setVisibility(price > 0); !result)
      return std::unexpected(std::move(result.error()));
  }

  auto item_icon = widget->fetchChild<ItemSlotWidget>("itemIcon");
  if (!item_icon)
    return std::unexpected(std::move(item_icon.error()));
  (*item_icon)->setItem(item);
  return widget->show();
}

std::expected<std::shared_ptr<Pane>, StarException> CraftingPane::setup_tooltip(ItemRecipe const& recipe) {
  auto inventory = capture_recipe_inventory();
  if (!inventory)
    return std::unexpected(std::move(inventory.error()));

  auto& root = context().root();
  auto tooltip = Pane::create(context());
  if (!tooltip)
    return std::unexpected(std::move(tooltip.error()));
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/interface/craftingtooltip/craftingtooltip.config");
  if (!config)
    return std::unexpected(std::move(config.error()));
  GuiReader reader(context());
  if (auto result = reader.construct(std::move(*config), **tooltip); !result)
    return std::unexpected(std::move(result.error()));

  auto list_widget = (*tooltip)->fetchChild<ListWidget>("itemList");
  if (!list_widget)
    return std::unexpected(std::move(list_widget.error()));
  auto& list = **list_widget;
  if (auto result = list.clear(); !result)
    return std::unexpected(std::move(result.error()));

  auto add_ingredient = [&](std::shared_ptr<Item> const& item, std::uint64_t available_count, std::uint64_t required_count)
      -> std::expected<void, StarException> {
    auto widget = list.addItem();
    if (!widget)
      return std::unexpected(std::move(widget.error()));
    auto item_name = (*widget)->fetchChild<LabelWidget>("itemName");
    if (!item_name)
      return std::unexpected(std::move(item_name.error()));
    if (auto result = (*item_name)->setText(item->friendly_name()); !result)
      return std::unexpected(std::move(result.error()));
    auto count = (*widget)->fetchChild<LabelWidget>("count");
    if (!count)
      return std::unexpected(std::move(count.error()));
    if (auto result = (*count)->setText(Star::format("{}/{}", available_count, required_count)); !result)
      return std::unexpected(std::move(result.error()));
    (*count)->setColor(available_count < required_count ? Color::palette_red : Color::palette_green);
    auto icon = (*widget)->fetchChild<ItemSlotWidget>("itemIcon");
    if (!icon)
      return std::unexpected(std::move(icon.error()));
    (*icon)->setItem(item);
    return (*widget)->show();
  };

  auto database = root.itemDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  if (!recipe.currency_inputs.empty()) {
    auto currencies = (*assets)->json("/currencies.config");
    if (!currencies)
      return std::unexpected(std::move(currencies.error()));
    for (auto const& [currency, count] : recipe.currency_inputs) {
      if (count == 0)
        continue;
      auto currency_config = currencies->get(currency);
      if (!currency_config)
        return std::unexpected(std::move(currency_config.error()));
      auto representative = currency_config->get_string("representativeItem");
      if (!representative)
        return std::unexpected(std::move(representative.error()));
      auto descriptor = ItemDescriptor::create(std::move(*representative));
      if (!descriptor)
        return std::unexpected(std::move(descriptor.error()));
      auto item = (*database)->item_shared(*descriptor);
      if (!item)
        return std::unexpected(std::move(item.error()));
      if (auto result = add_ingredient(*item, inventory->balances.value(currency, 0), count); !result)
        return std::unexpected(std::move(result.error()));
    }
  }

  for (auto const& input : recipe.inputs) {
    auto item = (*database)->item_shared(input.singular());
    if (!item)
      return std::unexpected(std::move(item.error()));
    auto available = ItemDatabase::get_count_of_item(inventory->stock, input, recipe.match_input_parameters);
    if (auto result = add_ingredient(*item, available, input.count()); !result)
      return std::unexpected(std::move(result.error()));
  }

  Vec2I row_height(0, list.size()[1]);
  auto background = (*tooltip)->fetchChild<ImageStretchWidget>("background");
  if (!background)
    return std::unexpected(std::move(background.error()));
  (*background)->setSize((*background)->size() + row_height);
  auto title = (*tooltip)->fetchChild<LabelWidget>("title");
  if (!title)
    return std::unexpected(std::move(title.error()));
  (*title)->setPosition((*title)->position() + row_height);
  (*tooltip)->setSize((*background)->size());
  return tooltip;
}

void CraftingPane::stop_crafting() {
  if (m_crafting_sound)
    m_crafting_sound->stop();
  m_crafting = false;
}

std::expected<void, StarException> CraftingPane::toggle_craft() {
  if (m_crafting) {
    stop_crafting();
    return {};
  }

  auto recipe = selected_recipe();
  if (!recipe)
    return std::unexpected(std::move(recipe.error()));
  if (!*recipe)
    return {};
  float duration = (**recipe).get().duration;
  if (!(duration > 0))
    return craft(m_count);
  auto disable_timer = m_settings.get_bool("disableTimer", false);
  if (!disable_timer)
    return std::unexpected(std::move(disable_timer.error()));
  if (*disable_timer)
    return craft(m_count);

  auto batch_size = m_settings.get_int("craftCount", 1);
  if (!batch_size)
    return std::unexpected(std::move(batch_size.error()));
  auto sound = m_settings.opt_string("craftingSound");
  if (!sound)
    return std::unexpected(std::move(sound.error()));
  if (*sound) {
    auto assets = context().root().assets();
    if (!assets)
      return std::unexpected(std::move(assets.error()));
    auto audio = (*assets)->audio(**sound);
    if (!audio)
      return std::unexpected(std::move(audio.error()));
    auto instance = AudioInstance::create(**audio);
    if (!instance)
      return std::unexpected(std::move(instance.error()));
    (*instance)->set_loops(-1);
    auto played = context().playAudio(*instance);
    if (!played)
      return std::unexpected(std::move(played.error()));
    m_crafting_sound = std::move(*instance);
  }
  m_batch_size = static_cast<int>(std::clamp<std::int64_t>(*batch_size, 0, std::numeric_limits<int>::max()));
  m_craft_timer = GameTimer(duration);
  m_crafting = true;
  return {};
}

std::expected<void, StarException> CraftingPane::craft(int count) {
  auto recipe = selected_recipe();
  if (!recipe)
    return std::unexpected(std::move(recipe.error()));
  if (!*recipe || count <= 0) {
    stop_crafting();
    return {};
  }

  auto database = context().root().itemDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto const& selected = (**recipe).get();
  auto crafted = (*database)->craft_recipe(*m_player, selected, static_cast<std::uint64_t>(count));
  if (!crafted)
    return std::unexpected(std::move(crafted.error()));
  if (!*crafted) {
    stop_crafting();
    return refresh_recipes(false);
  }

  m_blueprints->markAsRead(selected.output.singular());
  m_count -= count;
  if (m_count <= 0) {
    m_count = 1;
    stop_crafting();
  }
  if (auto result = count_changed(); !result)
    return std::unexpected(std::move(result.error()));
  return refresh_recipes(false);
}

std::expected<void, StarException> CraftingPane::count_text_changed(RecipeInventoryView inventory) {
  if (!m_text_box) {
    m_count = 1;
    return {};
  }
  auto text = m_text_box->getText().replace("x", "");
  if (!text)
    return std::unexpected(std::move(text.error()));
  if (text->empty()) {
    m_count = 1;
    return {};
  }

  auto count = lexical_cast<int>(*text);
  if (count) {
    auto limit = max_craft(inventory);
    if (!limit)
      return std::unexpected(std::move(limit.error()));
    m_count = std::clamp(*count, 1, std::max(1, *limit));
  } else if (!count.error().is<BadLexicalCast>()) {
    return std::unexpected(std::move(count.error()));
  } else {
    m_count = 1;
  }
  return count_changed();
}

std::expected<void, StarException> CraftingPane::count_changed() {
  if (m_text_box) {
    if (auto result = m_text_box->setText(Star::format("x{}", m_count), false); !result)
      return std::unexpected(std::move(result.error()));
  }
  return {};
}

std::expected<CraftingPane::RecipeListing, StarException> CraftingPane::determine_recipes(RecipeInventory const& inventory) {
  auto database = context().root().itemDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));

  StringSet category_filter;
  auto categories = fetchChild<ButtonGroupWidget>("categories");
  if (!categories)
    return std::unexpected(std::move(categories.error()));
  if (*categories) {
    if (auto button = (*categories)->checkedButton()) {
      auto groups = button->get().data().get_array("filter");
      if (!groups)
        return std::unexpected(std::move(groups.error()));
      auto names = json_to_string_set(*groups);
      if (!names)
        return std::unexpected(std::move(names.error()));
      category_filter = std::move(*names);
    }
  }

  HashSet<Rarity> rarity_filter;
  auto rarities = fetchChild<ButtonGroupWidget>("rarities");
  if (!rarities)
    return std::unexpected(std::move(rarities.error()));
  if (*rarities) {
    if (auto button = (*rarities)->checkedButton()) {
      auto values = button->get().data().get_array("rarity");
      if (!values)
        return std::unexpected(std::move(values.error()));
      for (auto const& value : *values) {
        auto name = value.to_string();
        if (!name)
          return std::unexpected(std::move(name.error()));
        auto rarity = enum_value<rarity_names>(*name);
        if (!rarity)
          return std::unexpected(std::move(rarity.error()));
        rarity_filter.add(*rarity);
      }
    }
  }

  auto filter_widget = fetchChild<TextBoxWidget>("filter");
  if (!filter_widget)
    return std::unexpected(std::move(filter_widget.error()));
  String filter_text = *filter_widget ? (*filter_widget)->getText() : String();

  bool admin = inventory.admin;
  bool require_materials = m_filter_have_materials && m_filter_have_materials->isChecked();
  HashSet<ItemRecipe> recipes;
  auto printer = m_settings.get_bool("printer", false);
  if (!printer)
    return std::unexpected(std::move(printer.error()));
  if (*printer) {
    auto objects = context().root().objectDatabase();
    if (!objects)
      return std::unexpected(std::move(objects.error()));
    auto print_time = m_settings.get_float("printTime", 0);
    if (!print_time)
      return std::unexpected(std::move(print_time.error()));
    auto print_factor = m_settings.get_float("printCostFactor", 1.0);
    if (!print_factor)
      return std::unexpected(std::move(print_factor.error()));
    auto names = admin ? (*objects)->allObjects() : StringList::from(m_player->log()->scannedObjects());
    for (auto const& name : names) {
      if (!(*objects)->isObject(name))
        continue;
      auto config = (*objects)->getConfig(name);
      if (!config)
        return std::unexpected(std::move(config.error()));
      if (!*config || !(*config)->printable || !(*database)->has_item(name))
        continue;

      auto output = ItemDescriptor::create(name, 1);
      if (!output)
        return std::unexpected(std::move(output.error()));
      auto item = (*database)->item_shared(*output);
      if (!item)
        return std::unexpected(std::move(item.error()));
      auto price = static_cast<double>((*item)->price()) * *print_factor;
      if (!(price >= 0 && price < 0x1p64))
        return std::unexpected(RecipeException("Printer currency requirement exceeds supported range"));
      auto filter = ItemDatabase::gui_filter_string(*item);
      if (!filter)
        return std::unexpected(std::move(filter.error()));

      ItemRecipe recipe;
      recipe.output = std::move(*output);
      recipe.currency_inputs["money"] = static_cast<std::uint64_t>(price);
      recipe.output_rarity = (*item)->rarity();
      recipe.duration = *print_time;
      recipe.gui_filter_string = std::move(*filter);
      recipe.groups = StringSet{(*config)->category};
      recipes.add(std::move(recipe));
    }
  } else {
    auto has_configured_recipes = m_settings.contains("recipes");
    if (!has_configured_recipes)
      return std::unexpected(std::move(has_configured_recipes.error()));
    if (*has_configured_recipes) {
      auto entries = m_settings.get_array("recipes");
      if (!entries)
        return std::unexpected(std::move(entries.error()));
      for (auto const& entry : *entries) {
        if (entry.type() == Json::Type::String) {
          auto name = entry.to_string();
          if (!name)
            return std::unexpected(std::move(name.error()));
          recipes.insert_range((*database)->recipes_for_output_item(*name));
        } else {
          auto recipe = (*database)->parse_recipe(entry);
          if (!recipe)
            return std::unexpected(std::move(recipe.error()));
          recipes.add(std::move(*recipe));
        }
      }
      if (require_materials)
        recipes = ItemDatabase::recipes_from_subset(inventory.stock, inventory.balances, recipes, m_filter);
    } else if (require_materials) {
      recipes = (*database)->recipes_from_bag_contents(inventory.stock, inventory.balances, m_filter);
    } else {
      recipes = (*database)->all_recipes(m_filter);
    }
  }

  bool requires_blueprint = false;
  if (!admin) {
    auto required = m_settings.get_bool("requiresBlueprint", true);
    if (!required)
      return std::unexpected(std::move(required.error()));
    requires_blueprint = *required;
  }

  StringList search_terms;
  if (!filter_text.empty()) {
    auto lower = filter_text.to_lower();
    if (!lower)
      return std::unexpected(std::move(lower.error()));
    auto terms = lower->split_any(" ,.?*\\+/|\t");
    if (!terms)
      return std::unexpected(std::move(terms.error()));
    search_terms = std::move(*terms);
  }

  List<ItemRecipe> sorted_recipes;
  for (auto const& recipe : recipes) {
    if ((requires_blueprint && !m_blueprints->isKnown(recipe.output)) ||
        (!category_filter.empty() && !recipe.groups.has_intersection(category_filter)) ||
        (!rarity_filter.empty() && (!recipe.output || !rarity_filter.contains(recipe.output_rarity))) ||
        (!filter_text.empty() && !recipe.output))
      continue;
    bool matches = true;
    for (auto const& term : search_terms) {
      auto match = recipe.gui_filter_string.contains(term);
      if (!match)
        return std::unexpected(std::move(match.error()));
      if (!*match) {
        matches = false;
        break;
      }
    }
    if (matches)
      sorted_recipes.push_back(recipe);
  }

  auto sorted = sort_by_key(sorted_recipes, [&](ItemRecipe const& recipe) -> std::expected<std::tuple<String, String>, StarException> {
    auto name = (*database)->item_friendly_name(recipe.output.name());
    if (!name)
      return std::unexpected(std::move(name.error()));
    auto trimmed = name->trim();
    if (!trimmed)
      return std::unexpected(std::move(trimmed.error()));
    auto lower = trimmed->to_lower();
    if (!lower)
      return std::unexpected(std::move(lower.error()));
    return std::make_tuple(std::move(*lower), recipe.output.name());
  });
  if (!sorted)
    return std::unexpected(std::move(sorted.error()));
  return RecipeListing{std::move(sorted_recipes), !*printer && require_materials};
}

std::expected<int, StarException> CraftingPane::max_craft(RecipeInventoryView inventory) const {
  auto recipe = selected_recipe();
  if (!recipe)
    return std::unexpected(std::move(recipe.error()));
  bool admin = inventory ? inventory->get().admin : m_player->isAdmin();
  if (!*recipe)
    return admin ? m_max_spin_count : 0;

  auto const& selected = (**recipe).get();
  std::uint64_t limit = std::numeric_limits<std::uint64_t>::max();
  if (admin) {
    if (selected.output.count())
      limit /= selected.output.count();
  } else if (inventory) {
    limit = ItemDatabase::max_craftable_in_bag(inventory->get().stock, inventory->get().balances, selected);
  } else {
    auto current = capture_recipe_inventory();
    if (!current)
      return std::unexpected(std::move(current.error()));
    limit = ItemDatabase::max_craftable_in_bag(current->stock, current->balances, selected);
  }
  return static_cast<int>(std::min<std::uint64_t>(limit, std::max(0, m_max_spin_count)));
}

std::expected<CraftingPane::SelectedRecipe, StarException> CraftingPane::selected_recipe() const {
  if (auto widget = m_gui_list->selectedWidget()) {
    auto recipe = m_recipe_widgets.get_left(widget);
    if (!recipe)
      return std::unexpected(std::move(recipe.error()));
    return SelectedRecipe(*recipe);
  }
  return SelectedRecipe{};
}

}
