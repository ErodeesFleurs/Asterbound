import star.json_extra;
import std;
import star.output_proxy;
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
import star.logging;
import star.io_device;
import star.ref_ptr;
import star.format;
import star.version;
import star.periodic_function;
import star.image;
import star.math_common;
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
import star.animation;
import star.particle;
import star.animated_part_set;
import star.networked_animator;
import star.humanoid;


import star.char_creation;
import star.widget_parsing;
import star.gui_reader;
import star.liquid_types;
import star.tile_sector_array;
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
import star.button_group;
import star.button_widget;
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
import star.portrait_widget;
import star.text_box_widget;
import star.label_widget;
import star.image_widget;
import star.status_effect_item;
import star.effect_source_item;
import star.previewable_item;
import star.tool_user_item;
import star.fireable_item;
import star.swingable_item;
import star.armors;
import star.player_log;
import star.name_generator;

import star.species_database;


import star.player_inventory;
import star.player_factory;

namespace Star {

CharCreationPane::CharCreationPane(GuiContext& guiContext)
  : Pane(guiContext), m_speciesChoice(0), m_genderChoice(0), m_modeChoice(1), m_bodyColor(0), m_alty(0),
    m_hairChoice(0), m_heady(0), m_shirtChoice(0), m_shirtColor(0), m_pantsChoice(0), m_pantsColor(0),
    m_personality(0) {}

std::expected<std::shared_ptr<CharCreationPane>, StarException> CharCreationPane::create(GuiContext& guiContext,
    std::function<std::expected<void, StarException>(std::shared_ptr<Player>)> requestCloseFunc) {
  if (!requestCloseFunc)
    return std::unexpected(CharCreationException("Character creation requires a close callback"));
  auto pane = std::shared_ptr<CharCreationPane>(new CharCreationPane(guiContext));
  if (auto result = pane->initializePane(); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = pane->init(std::move(requestCloseFunc)); !result)
    return std::unexpected(std::move(result.error()));
  return pane;
}

std::expected<void, StarException> CharCreationPane::init(
    std::function<std::expected<void, StarException>(std::shared_ptr<Player>)> requestCloseFunc) {
  auto assets = context().root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto speciesConfig = (*assets)->json("/interface/windowconfig/charcreation.config:speciesOrdering");
  if (!speciesConfig)
    return std::unexpected(std::move(speciesConfig.error()));
  auto speciesList = jsonToStringList(*speciesConfig);
  if (!speciesList)
    return std::unexpected(std::move(speciesList.error()));
  m_speciesList = std::move(*speciesList);
  if (m_speciesList.empty())
    return std::unexpected(CharCreationException("Character creation species ordering is empty"));
  GuiReader guiReader(context());
  guiReader.registerCallback("cancel", [requestCloseFunc](Widget&) { return requestCloseFunc({}); });
  guiReader.registerCallback("saveChar", [this, requestCloseFunc](Widget&) -> std::expected<void, StarException> {
    auto skipIntro = requireChild<ButtonWidget>("btnSkipIntro");
    if (!skipIntro)
      return std::unexpected(std::move(skipIntro.error()));
    if ((*skipIntro)->isChecked())
      m_previewPlayer->log()->setIntroComplete(true);
    if (auto result = requestCloseFunc(m_previewPlayer); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = createPlayer(); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = randomize(); !result)
      return std::unexpected(std::move(result.error()));
    return randomizeName();
  });
  for (auto const& [callback, member, increment, clothing] : {
      std::tuple{std::string_view("mainSkinColor.up"), &CharCreationPane::m_bodyColor, true, false},
      std::tuple{std::string_view("mainSkinColor.down"), &CharCreationPane::m_bodyColor, false, false},
      std::tuple{std::string_view("alty.up"), &CharCreationPane::m_alty, true, false},
      std::tuple{std::string_view("alty.down"), &CharCreationPane::m_alty, false, false},
      std::tuple{std::string_view("hairStyle.up"), &CharCreationPane::m_hairChoice, true, false},
      std::tuple{std::string_view("hairStyle.down"), &CharCreationPane::m_hairChoice, false, false},
      std::tuple{std::string_view("shirt.up"), &CharCreationPane::m_shirtChoice, true, true},
      std::tuple{std::string_view("shirt.down"), &CharCreationPane::m_shirtChoice, false, true},
      std::tuple{std::string_view("pants.up"), &CharCreationPane::m_pantsChoice, true, true},
      std::tuple{std::string_view("pants.down"), &CharCreationPane::m_pantsChoice, false, true},
      std::tuple{std::string_view("heady.up"), &CharCreationPane::m_heady, true, false},
      std::tuple{std::string_view("heady.down"), &CharCreationPane::m_heady, false, false},
      std::tuple{std::string_view("shirtColor.up"), &CharCreationPane::m_shirtColor, true, true},
      std::tuple{std::string_view("shirtColor.down"), &CharCreationPane::m_shirtColor, false, true},
      std::tuple{std::string_view("pantsColor.up"), &CharCreationPane::m_pantsColor, true, true},
      std::tuple{std::string_view("pantsColor.down"), &CharCreationPane::m_pantsColor, false, true},
      std::tuple{std::string_view("personality.up"), &CharCreationPane::m_personality, true, false},
      std::tuple{std::string_view("personality.down"), &CharCreationPane::m_personality, false, false}}) {
    guiReader.registerCallback(callback, [this, member, increment, clothing](Widget&) -> std::expected<void, StarException> {
      if (increment)
        ++(this->*member);
      else
        --(this->*member);
      if (clothing) {
        auto toggle = requireChild<ButtonWidget>("btnToggleClothing");
        if (!toggle)
          return std::unexpected(std::move(toggle.error()));
        (*toggle)->setChecked(true);
      }
      return changed();
    });
  }
  guiReader.registerCallback("toggleClothing", [this](Widget&) { return changed(); });
  guiReader.registerCallback("randomName", [this](Widget&) { return randomizeName(); });
  guiReader.registerCallback("randomize", [this](Widget&) { return randomize(); });
  guiReader.registerCallback("name", [this](Widget& object) { return nameBoxCallback(object); });
  guiReader.registerCallback("species", [this](Widget& widget) -> std::expected<void, StarException> {
    auto button = rconvert<ButtonWidget>(widget);
    if (!button)
      return std::unexpected(std::move(button.error()));
    std::size_t choice = button->get().buttonGroupId();
    if (choice < m_speciesList.size() && choice != m_speciesChoice) {
      m_speciesChoice = choice;
      if (auto result = randomize(); !result)
        return std::unexpected(std::move(result.error()));
      return randomizeName();
    }
    return {};
  });
  guiReader.registerCallback("gender", [this](Widget& widget) -> std::expected<void, StarException> {
    auto button = rconvert<ButtonWidget>(widget);
    if (!button)
      return std::unexpected(std::move(button.error()));
    m_genderChoice = button->get().buttonGroupId();
    return changed();
  });
  guiReader.registerCallback("mode", [this](Widget& widget) -> std::expected<void, StarException> {
    auto button = rconvert<ButtonWidget>(widget);
    if (!button)
      return std::unexpected(std::move(button.error()));
    m_modeChoice = button->get().buttonGroupId();
    return changed();
  });
  auto layout = (*assets)->json("/interface/windowconfig/charcreation.config:paneLayout");
  if (!layout)
    return std::unexpected(std::move(layout.error()));
  if (auto result = guiReader.construct(*layout, *this); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = createPlayer(); !result)
    return std::unexpected(std::move(result.error()));
  RandomSource random(context().root().random().randu64());
  m_speciesChoice = random.randu32() % m_speciesList.size();
  m_genderChoice = random.randu32();
  m_modeChoice = 1;
  if (auto result = randomize(); !result)
    return std::unexpected(std::move(result.error()));
  return randomizeName();
}

std::expected<void, StarException> CharCreationPane::createPlayer() {
  auto factory = context().root().playerFactory();
  if (!factory)
    return std::unexpected(std::move(factory.error()));
  auto player = (*factory)->create();
  if (!player)
    return std::unexpected(std::move(player.error()));
  m_previewPlayer = std::move(*player);
  auto portrait = fetchChild<PortraitWidget>("charPreview");
  if (!portrait)
    return std::unexpected(std::move(portrait.error()));
  if (*portrait)
    return (*portrait)->setEntity(m_previewPlayer);
  auto error = CharCreationException("The charPreview portrait has the wrong type.");
  return context().root().logger().error("Character Preview portrait was not found in the json specification. {}", outputException(ExceptionDiagnostic(error), false));
}

std::expected<void, StarException> CharCreationPane::randomize() {
  RandomSource random(context().root().random().randu64());
  m_bodyColor = random.randu32();
  m_hairChoice = random.randu32();
  m_alty = random.randu32();
  m_heady = random.randu32();
  m_shirtChoice = random.randu32();
  m_shirtColor = random.randu32();
  m_pantsChoice = random.randu32();
  m_pantsColor = random.randu32();
  m_personality = random.randu32();
  return changed();
}

std::expected<void, StarException> CharCreationPane::tick(float dt) {
  if (auto result = Pane::tick(dt); !result)
    return std::unexpected(std::move(result.error()));
  if (!active() || !m_previewPlayer)
    return {};
  return m_previewPlayer->animatePortrait(dt);
}

std::expected<bool, StarException> CharCreationPane::sendEvent(InputEvent const& event) {
  if (active() && m_previewPlayer) {
    if (std::holds_alternative<KeyDownEvent>(event)) {
      auto actions = context().actions(event);
      if (actions.contains(InterfaceAction::EmoteBlabbering))
        m_previewPlayer->addEmote(HumanoidEmote::Blabbering);
      if (actions.contains(InterfaceAction::EmoteShouting))
        m_previewPlayer->addEmote(HumanoidEmote::Shouting);
      if (actions.contains(InterfaceAction::EmoteHappy))
        m_previewPlayer->addEmote(HumanoidEmote::Happy);
      if (actions.contains(InterfaceAction::EmoteSad))
        m_previewPlayer->addEmote(HumanoidEmote::Sad);
      if (actions.contains(InterfaceAction::EmoteNeutral))
        m_previewPlayer->addEmote(HumanoidEmote::NEUTRAL);
      if (actions.contains(InterfaceAction::EmoteLaugh))
        m_previewPlayer->addEmote(HumanoidEmote::Laugh);
      if (actions.contains(InterfaceAction::EmoteAnnoyed))
        m_previewPlayer->addEmote(HumanoidEmote::Annoyed);
      if (actions.contains(InterfaceAction::EmoteOh))
        m_previewPlayer->addEmote(HumanoidEmote::Oh);
      if (actions.contains(InterfaceAction::EmoteOooh))
        m_previewPlayer->addEmote(HumanoidEmote::OOOH);
      if (actions.contains(InterfaceAction::EmoteBlink))
        m_previewPlayer->addEmote(HumanoidEmote::Blink);
      if (actions.contains(InterfaceAction::EmoteWink))
        m_previewPlayer->addEmote(HumanoidEmote::Wink);
      if (actions.contains(InterfaceAction::EmoteEat))
        m_previewPlayer->addEmote(HumanoidEmote::Eat);
      if (actions.contains(InterfaceAction::EmoteSleep))
        m_previewPlayer->addEmote(HumanoidEmote::Sleep);
    }
  }
  return Pane::sendEvent(event);
}

std::expected<void, StarException> CharCreationPane::randomizeName() {
  auto database = context().root().speciesDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto species = (*database)->species(m_speciesList[m_speciesChoice]);
  if (!species)
    return std::unexpected(std::move(species.error()));
  auto textBox = requireChild<TextBoxWidget>("name");
  if (!textBox)
    return std::unexpected(std::move(textBox.error()));
  auto genderOption = (*species)->options().genderOptions.wrap(m_genderChoice);
  if (!genderOption)
    return std::unexpected(std::move(genderOption.error()));
  auto generator = context().root().nameGenerator();
  if (!generator)
    return std::unexpected(std::move(generator.error()));
  int limiter = 100;
  while (true) {
    auto name = (*generator)->generateName((*species)->nameGen(genderOption->get().gender));
    if (!name)
      return std::unexpected(std::move(name.error()));
    auto accepted = (*textBox)->setText(*name);
    if (!accepted)
      return std::unexpected(std::move(accepted.error()));
    if (*accepted || limiter == 0)
      break;
    --limiter;
  }
  return changed();
}

std::expected<void, StarException> CharCreationPane::changed() {
  auto& root = context().root();
  auto setRequiredLabel = [this](String const& name, String const& text) -> std::expected<void, StarException> {
    auto label = requireChild<LabelWidget>(name);
    if (!label)
      return std::unexpected(std::move(label.error()));
    return (*label)->setText(text);
  };
  auto textBox = requireChild<TextBoxWidget>("name");
  if (!textBox)
    return std::unexpected(std::move(textBox.error()));
  auto database = root.speciesDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto speciesDefinition = (*database)->species(m_speciesList[m_speciesChoice]);
  if (!speciesDefinition)
    return std::unexpected(std::move(speciesDefinition.error()));
  auto const& species = (*speciesDefinition)->options();
  auto genderOptions = species.genderOptions.wrap(m_genderChoice);
  if (!genderOptions)
    return std::unexpected(std::move(genderOptions.error()));
  int genderIdx = pmod<std::int64_t>(m_genderChoice, species.genderOptions.size());
  auto const& labels = (*speciesDefinition)->charGenTextLabels();
  for (auto const& [name, index] : {
      std::pair{std::string_view("labelMainSkinColor"), 0}, std::pair{std::string_view("labelHairStyle"), 1},
      std::pair{std::string_view("labelShirt"), 2}, std::pair{std::string_view("labelPants"), 3}, std::pair{std::string_view("labelHeady"), 5},
      std::pair{std::string_view("labelShirtColor"), 6}, std::pair{std::string_view("labelPantsColor"), 7},
      std::pair{std::string_view("labelPortrait"), 8}, std::pair{std::string_view("labelPersonality"), 9}}) {
    auto label = labels.at(index);
    if (!label)
      return std::unexpected(std::move(label.error()));
    if (auto result = setRequiredLabel(name, label->get()); !result)
      return std::unexpected(std::move(result.error()));
  }
  auto altyLabel = labels.at(4);
  if (!altyLabel)
    return std::unexpected(std::move(altyLabel.error()));
  if (!altyLabel->get().empty()) {
    if (auto result = setRequiredLabel("labelAlty", altyLabel->get()); !result)
      return std::unexpected(std::move(result.error()));
  }
  for (std::string_view name : {std::string_view("labelAlty"), std::string_view("alty")}) {
    auto child = requireChild(name);
    if (!child)
      return std::unexpected(std::move(child.error()));
    if (auto result = (*child)->setVisibility(!altyLabel->get().empty()); !result)
      return std::unexpected(std::move(result.error()));
  }
  auto speciesButton = fetchChild<ButtonWidget>(strf("species.{}", m_speciesChoice));
  if (!speciesButton)
    return std::unexpected(std::move(speciesButton.error()));
  if (*speciesButton) {
    if (auto result = (*speciesButton)->check(); !result)
      return std::unexpected(std::move(result.error()));
  }
  auto genderButton = fetchChild<ButtonWidget>(strf("gender.{}", genderIdx));
  if (!genderButton)
    return std::unexpected(std::move(genderButton.error()));
  if (*genderButton) {
    if (auto result = (*genderButton)->check(); !result)
      return std::unexpected(std::move(result.error()));
  }
  auto modeButton = requireChild<ButtonWidget>(strf("mode.{}", m_modeChoice));
  if (!modeButton)
    return std::unexpected(std::move(modeButton.error()));
  if (auto result = (*modeButton)->check(); !result)
    return std::unexpected(std::move(result.error()));
  auto description = (*modeButton)->data().getString("description", "fail");
  if (!description)
    return std::unexpected(std::move(description.error()));
  if (auto result = setRequiredLabel("labelMode", *description); !result)
    return std::unexpected(std::move(result.error()));
  for (std::size_t i = 0; i < species.genderOptions.size(); ++i) {
    auto button = fetchChild<ButtonWidget>(strf("gender.{}", i));
    if (!button)
      return std::unexpected(std::move(button.error()));
    if (*button)
      (*button)->setOverlayImage(species.genderOptions[i].image);
  }
  for (auto const& [name, definition] : (*database)->allSpecies()) {
    if (auto it = std::ranges::find(m_speciesList, name); it != m_speciesList.end()) {
      auto button = fetchChild<ButtonWidget>(strf("species.{}", static_cast<std::size_t>(it - m_speciesList.begin())));
      if (!button)
        return std::unexpected(std::move(button.error()));
      if (*button) {
        auto option = definition->options().genderOptions.at(genderIdx);
        if (!option)
          return std::unexpected(std::move(option.error()));
        (*button)->setOverlayImage(option->get().characterImage);
      }
    }
  }
  auto portrait = requireChild<PortraitWidget>("charPreview");
  if (!portrait)
    return std::unexpected(std::move(portrait.error()));
  auto clothing = requireChild<ButtonWidget>("btnToggleClothing");
  if (!clothing)
    return std::unexpected(std::move(clothing.error()));
  if (auto result = (*portrait)->setMode((*clothing)->isChecked() ? PortraitMode::Full : PortraitMode::FullNude); !result)
    return std::unexpected(std::move(result.error()));
  auto results = (*database)->createHumanoid((*textBox)->getText(), species.species, m_genderChoice, m_bodyColor,
      m_alty, m_hairChoice, m_heady, m_shirtChoice, m_shirtColor, m_pantsChoice, m_pantsColor, m_personality);
  if (!results)
    return std::unexpected(std::move(results.error()));
  if (auto result = m_previewPlayer->setModeType(static_cast<PlayerMode>(m_modeChoice)); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_previewPlayer->setHumanoidParameters(std::move(results->humanoidParameters)); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_previewPlayer->setIdentity(std::move(results->identity)); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_previewPlayer->refreshHumanoidParameters(); !result)
    return std::unexpected(std::move(result.error()));
  if (!EquipmentSlotNames)
    return std::unexpected(EquipmentSlotNames.error());
  auto itemDatabase = root.itemDatabase();
  if (!itemDatabase)
    return std::unexpected(std::move(itemDatabase.error()));
  for (auto const& slot : *EquipmentSlotNames) {
    if (auto equipment = results->armor.maybe(slot.second)) {
      auto descriptor = ItemDescriptor::create(*equipment);
      if (!descriptor)
        return std::unexpected(std::move(descriptor.error()));
      auto item = (*itemDatabase)->item(*descriptor);
      if (!item)
        return std::unexpected(std::move(item.error()));
      if (auto result = m_previewPlayer->inventory()->setItem(InventorySlot(slot.first), *item); !result)
        return std::unexpected(std::move(result.error()));
    } else if (auto result = m_previewPlayer->inventory()->consumeSlot(InventorySlot(slot.first)); !result) {
      return std::unexpected(std::move(result.error()));
    }
  }
  if (auto result = m_previewPlayer->refreshEquipment(); !result)
    return std::unexpected(std::move(result.error()));
  return m_previewPlayer->finalizeCreation();
}

std::expected<void, StarException> CharCreationPane::nameBoxCallback(Widget& object) {
  if (!asRef<TextBoxWidget>(object))
    return std::unexpected(GuiException("Invalid object type, expected TextBoxWidget."));
  return changed();
}

std::expected<std::shared_ptr<Pane>, StarException> CharCreationPane::createTooltip(Vec2I const& screenPosition) {
  if (auto child = getChildAt(screenPosition)) {
    if (auto parent = child->parent(); parent && parent->get().name() == "species") {
      auto button = convert<ButtonWidget>(child);
      if (!button)
        return std::unexpected(std::move(button.error()));
      std::size_t speciesIndex = (*button)->buttonGroupId();
      if (speciesIndex >= m_speciesList.size())
        return std::shared_ptr<Pane>{};
      auto database = context().root().speciesDatabase();
      if (!database)
        return std::unexpected(std::move(database.error()));
      auto definition = (*database)->species(m_speciesList[speciesIndex]);
      if (!definition)
        return std::unexpected(std::move(definition.error()));
      auto tooltip = Pane::create(context());
      if (!tooltip)
        return std::unexpected(std::move(tooltip.error()));
      (*tooltip)->removeAllChildren();
      GuiReader reader(context());
      auto assets = context().root().assets();
      if (!assets)
        return std::unexpected(std::move(assets.error()));
      auto config = (*assets)->json("/interface/tooltips/species.tooltip");
      if (!config)
        return std::unexpected(std::move(config.error()));
      if (auto result = reader.construct(*config, **tooltip); !result)
        return std::unexpected(std::move(result.error()));
      auto genderOption = (*definition)->options().genderOptions.wrap(m_genderChoice);
      if (!genderOption)
        return std::unexpected(std::move(genderOption.error()));
      auto icon = ImageWidget::create(context(), genderOption->get().characterImage);
      if (!icon)
        return std::unexpected(std::move(icon.error()));
      if (auto result = (*tooltip)->setTitle(*icon, (*definition)->tooltip().title, (*definition)->tooltip().subTitle); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = (*tooltip)->setLabel("descriptionLabel", (*definition)->tooltip().description); !result)
        return std::unexpected(std::move(result.error()));
      return std::move(*tooltip);
    }
  }
  return std::shared_ptr<Pane>{};
}

}
