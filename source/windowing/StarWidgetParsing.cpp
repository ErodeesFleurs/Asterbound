import std;
import star.json;
import star.json_extra;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.vector;
import star.casting;
import star.input_event;
import star.color;
import star.font;
import star.bi_map;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.format;
import star.version;
import star.string;
import star.text;
import star.listener;
import star.game_types;
import star.set;
import star.audio;
import star.map;
import star.id_map;
import star.rpc_promise;
import star.strong_typedef;
import star.a_star;
import star.ordered_set;
import star.random;
import star.periodic_function;
import star.either;

import star.application;
import star.statistics_service;
import star.p2p_networking_service;
import star.user_generated_content_service;
import star.desktop_service;
import star.image;
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
import star.widget_parsing;
import star.pane;
import star.button_group;
import star.button_widget;
import star.canvas_widget;
import star.image_widget;
import star.image_stretch_widget;
import star.uuid;
import star.item_descriptor;
import star.particle;
import star.animated_part_set;
import star.light_source;
import star.networked_animator;
import star.humanoid;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.entity;
import star.interaction_types;
import star.tile_damage;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_modification;
import star.force_regions;
import star.world;
import star.physics_entity;
import star.anchorable_entity;
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.lounging_entities;
import star.chat_action;
import star.chatty_entity;
import star.emote_entity;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.inspectable_entity;
import star.inventory_types;
import star.movement_controller;
import star.platformer_astar_types;
import star.game_timers;
import star.actor_movement_controller;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.ai_types;
import star.entity_rendering_types;
import star.entity_rendering;
import star.player_types;
import star.lua_components;
import star.lua_actor_movement_component;
import star.lua_animation_component;
import star.radio_message_database;
import star.player;
import star.portrait_widget;
import star.fuel_widget;
import star.progress_widget;
import star.label_widget;
import star.large_char_plate_widget;
import star.text_box_widget;
import star.animation;
import star.item_slot_widget;
import star.item;
import star.item_grid_widget;
import star.gui_reader;
import star.list_widget;
import star.slider_bar;
import star.stack_widget;
import star.scroll_area;
import star.layout;
import star.flow_layout;
import star.vertical_layout;
import star.tab_set;


import star.image_metadata_database;

import star.algorithm;
namespace Star {

WidgetConstructResult::WidgetConstructResult() : zlevel() {}

WidgetConstructResult::WidgetConstructResult(std::shared_ptr<Widget> obj, String const& name, float zlevel)
  : obj(std::move(obj)), name(name), zlevel(zlevel) {}

WidgetParser::WidgetParser(GuiContext& context) : m_context(context) {
  m_constructors["widget"] = [&parser = *this](String const& name, Json const& config) { return parser.widgetHandler(name, config); };
  m_constructors["canvas"] = [&parser = *this](String const& name, Json const& config) { return parser.canvasHandler(name, config); };
  m_constructors["image"] = [&parser = *this](String const& name, Json const& config) { return parser.imageHandler(name, config); };
  m_constructors["imageStretch"] = [&parser = *this](String const& name, Json const& config) { return parser.imageStretchHandler(name, config); };
  m_constructors["label"] = [&parser = *this](String const& name, Json const& config) { return parser.labelHandler(name, config); };
  m_constructors["portrait"] = [&parser = *this](String const& name, Json const& config) { return parser.portraitHandler(name, config); };
  m_constructors["fuelGauge"] = [&parser = *this](String const& name, Json const& config) { return parser.fuelGaugeHandler(name, config); };
  m_constructors["progress"] = [&parser = *this](String const& name, Json const& config) { return parser.progressHandler(name, config); };
  m_constructors["largeCharPlate"] = [&parser = *this](String const& name, Json const& config) { return parser.largeCharPlateHandler(name, config); };
  m_constructors["container"] = [&parser = *this](String const& name, Json const& config) { return parser.containerHandler(name, config); };
  m_constructors["layout"] = [&parser = *this](String const& name, Json const& config) { return parser.layoutHandler(name, config); };
  m_callbacks["null"] = [](Widget&) -> std::expected<void, StarException> { return {}; };
}

std::expected<void, StarException> WidgetParser::construct(Json const& config, Widget& widget) {
  m_pane = as<Pane>(widget.weak_from_this().lock());
  return constructImpl(config, widget);
}

std::expected<void, StarException> WidgetParser::constructImpl(Json const& config, Widget& widget) {
  auto widgets = constructor(config);
  if (!widgets)
    return std::unexpected(std::move(widgets.error()));
  std::sort(widgets->begin(), widgets->end(), [](WidgetConstructResult const& a, WidgetConstructResult const& b) {
    if (a.zlevel == b.zlevel)
      return a.obj->position() < b.obj->position();
    return a.zlevel < b.zlevel;
  });
  for (auto const& res : *widgets) {
    auto added = widget.addChild(res.name, res.obj);
    if (!added)
      return std::unexpected(std::move(added.error()));
    auto focused = res.obj->hasFocus();
    if (!focused)
      return std::unexpected(std::move(focused.error()));
    if (auto pane = m_pane.lock(); *focused && pane) {
      auto focus = pane->setFocus(*res.obj);
      if (!focus)
        return std::unexpected(std::move(focus.error()));
    }
  }
  return {};
}

std::expected<std::shared_ptr<Widget>, StarException> WidgetParser::makeSingle(String const& name, Json const& config) {
  auto type = config.getString("type");
  if (!type)
    return std::unexpected(std::move(type.error()));
  auto handler = m_constructors.maybeRef(*type);
  if (!handler || !handler->get())
    return std::unexpected(WidgetParserException::format("Unknown type in gui json. {}", *type));
  auto result = (handler->get())(name, config);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return std::move(result->obj);
}

std::expected<List<WidgetConstructResult>, StarException> WidgetParser::constructor(Json const& config) {
  List<WidgetConstructResult> widgets;
  auto addWidget = [&parser = *this, &widgets](Json const& memberConfig) -> std::expected<void, StarException> {
    if (!memberConfig.isType(Json::Type::Object))
      return std::unexpected(WidgetParserException("Malformed gui json: member configuration is not a map"));
    auto type = memberConfig.getString("type");
    if (!type)
      return std::unexpected(std::move(type.error()));
    auto name = memberConfig.getString("name");
    if (!name)
      return std::unexpected(std::move(name.error()));
    if (*type == "include") {
      auto file = memberConfig.getString("file");
      if (!file)
        return std::unexpected(std::move(file.error()));
      auto assets = parser.m_context.root().assets();
      if (!assets)
        return std::unexpected(std::move(assets.error()));
      auto included = (*assets)->json(*file);
      if (!included)
        return std::unexpected(std::move(included.error()));
      auto children = parser.constructor(*included);
      if (!children)
        return std::unexpected(std::move(children.error()));
      widgets.appendAll(std::move(*children));
    } else {
      auto handler = parser.m_constructors.maybeRef(*type);
      if (!handler || !handler->get())
        return std::unexpected(WidgetParserException::format("Unknown type in gui json. {}", *type));
      auto result = (handler->get())(*name, memberConfig);
      if (!result)
        return std::unexpected(std::move(result.error()));
      if (result->obj)
        widgets.append(std::move(*result));
    }
    return {};
  };
  if (config.isType(Json::Type::Object)) {
    auto members = config.iterateObject();
    if (!members)
      return std::unexpected(std::move(members.error()));
    for (auto const& member : *members) {
      auto named = member.second.set("name", member.first);
      if (!named)
        return std::unexpected(std::move(named.error()));
      auto added = addWidget(*named);
      if (!added)
        return std::unexpected(std::move(added.error()));
    }
  } else if (config.isType(Json::Type::Array)) {
    auto members = config.iterateArray();
    if (!members)
      return std::unexpected(std::move(members.error()));
    for (auto const& member : *members) {
      auto added = addWidget(member);
      if (!added)
        return std::unexpected(std::move(added.error()));
    }
  } else {
    return std::unexpected(WidgetParserException::format("Malformed gui json, expected a Map or a List. Instead got {}", config.typeName()));
  }
  return widgets;
}

void WidgetParser::registerCallback(String const& name, WidgetCallbackFunc callback) {
  m_callbacks[name] = std::move(callback);
}

std::expected<WidgetConstructResult, StarException> WidgetParser::buttonHandler(String const& name, Json const& config) {
  auto invisible = config.getBool("invisible", false);
  if (!invisible)
    return std::unexpected(std::move(invisible.error()));
  String baseImage;
  if (!*invisible) {
    auto base = config.getString("base");
    if (!base)
      return std::unexpected(std::move(base.error()));
    baseImage = std::move(*base);
  }
  auto hover = config.getString("hover", "");
  if (!hover)
    return std::unexpected(std::move(hover.error()));
  auto pressed = config.getString("pressed", "");
  if (!pressed)
    return std::unexpected(std::move(pressed.error()));
  auto disabledImage = config.getString("disabledImage", "");
  if (!disabledImage)
    return std::unexpected(std::move(disabledImage.error()));
  auto baseImageChecked = config.getString("baseImageChecked", "");
  if (!baseImageChecked)
    return std::unexpected(std::move(baseImageChecked.error()));
  auto hoverImageChecked = config.getString("hoverImageChecked", "");
  if (!hoverImageChecked)
    return std::unexpected(std::move(hoverImageChecked.error()));
  auto pressedImageChecked = config.getString("pressedImageChecked", "");
  if (!pressedImageChecked)
    return std::unexpected(std::move(pressedImageChecked.error()));
  auto disabledImageChecked = config.getString("disabledImageChecked", "");
  if (!disabledImageChecked)
    return std::unexpected(std::move(disabledImageChecked.error()));
  auto callback = config.getString("callback", name);
  if (!callback)
    return std::unexpected(std::move(callback.error()));
  auto callbackFunc = m_callbacks.maybeRef(*callback);
  if (!callbackFunc || !callbackFunc->get())
    return std::unexpected(WidgetParserException::format("Failed to find callback named: {}", *callback));
  auto buttonResult = ButtonWidget::create(m_context, callbackFunc->get(), baseImage, *hover, *pressed, *disabledImage);
  if (!buttonResult)
    return std::unexpected(std::move(buttonResult.error()));
  auto button = std::move(*buttonResult);
  auto result1 = button->setCheckedImages(*baseImageChecked, *hoverImageChecked, *pressedImageChecked, *disabledImageChecked);
  if (!result1)
    return std::unexpected(std::move(result1.error()));
  auto result2 = common(button, config);
  if (!result2)
    return std::unexpected(std::move(result2.error()));
  button->setInvisible(*invisible);
  auto has3 = config.contains("caption");
  if (!has3)
    return std::unexpected(std::move(has3.error()));
  if (*has3) {
    auto value3 = config.getString("caption");
    if (!value3)
      return std::unexpected(std::move(value3.error()));
    button->setText(*value3);
  }
  auto has4 = config.contains("pressedOffset");
  if (!has4)
    return std::unexpected(std::move(has4.error()));
  if (*has4) {
    auto value4 = config.get("pressedOffset");
    if (!value4)
      return std::unexpected(std::move(value4.error()));
    auto converted4 = jsonToVec2I(*value4);
    if (!converted4)
      return std::unexpected(std::move(converted4.error()));
    button->setPressedOffset(*converted4);
  }
  auto has5 = config.contains("textOffset");
  if (!has5)
    return std::unexpected(std::move(has5.error()));
  if (*has5) {
    auto value5 = config.get("textOffset");
    if (!value5)
      return std::unexpected(std::move(value5.error()));
    auto converted5 = jsonToVec2I(*value5);
    if (!converted5)
      return std::unexpected(std::move(converted5.error()));
    button->setTextOffset(*converted5);
  }
  auto has6 = config.contains("checkable");
  if (!has6)
    return std::unexpected(std::move(has6.error()));
  if (*has6) {
    auto value6 = config.getBool("checkable");
    if (!value6)
      return std::unexpected(std::move(value6.error()));
    button->setCheckable(*value6);
  }
  auto has7 = config.contains("sustainCallbackOnDownHold");
  if (!has7)
    return std::unexpected(std::move(has7.error()));
  if (*has7) {
    auto value7 = config.getBool("sustainCallbackOnDownHold");
    if (!value7)
      return std::unexpected(std::move(value7.error()));
    button->setSustainCallbackOnDownHold(*value7);
  }
  auto has8 = config.contains("checked");
  if (!has8)
    return std::unexpected(std::move(has8.error()));
  if (*has8) {
    auto value8 = config.getBool("checked");
    if (!value8)
      return std::unexpected(std::move(value8.error()));
    button->setChecked(*value8);
  }
  auto hAnchor = config.getString("textAlign", "center");
  if (!hAnchor)
    return std::unexpected(std::move(hAnchor.error()));
  if (*hAnchor == "right")
    button->setTextAlign(HorizontalAnchor::RightAnchor);
  else if (*hAnchor == "center")
    button->setTextAlign(HorizontalAnchor::HMidAnchor);
  else if (*hAnchor == "left")
    button->setTextAlign(HorizontalAnchor::LeftAnchor);
  else
    return std::unexpected(WidgetParserException::format("Malformed gui json, expected textAlign to be left, right, or center, got {}", *hAnchor));
  auto has9 = config.contains("fontSize");
  if (!has9)
    return std::unexpected(std::move(has9.error()));
  if (*has9) {
    auto value9 = config.getInt("fontSize");
    if (!value9)
      return std::unexpected(std::move(value9.error()));
    button->setFontSize(*value9);
  }
  auto has10 = config.contains("fontDirectives");
  if (!has10)
    return std::unexpected(std::move(has10.error()));
  if (*has10) {
    auto value10 = config.getString("fontDirectives");
    if (!value10)
      return std::unexpected(std::move(value10.error()));
    button->setFontDirectives(*value10);
  }
  auto has11 = config.contains("fontColor");
  if (!has11)
    return std::unexpected(std::move(has11.error()));
  if (*has11) {
    auto value11 = config.get("fontColor");
    if (!value11)
      return std::unexpected(std::move(value11.error()));
    auto converted11 = jsonToColor(*value11);
    if (!converted11)
      return std::unexpected(std::move(converted11.error()));
    button->setFontColor(*converted11);
  }
  auto has12 = config.contains("fontColorDisabled");
  if (!has12)
    return std::unexpected(std::move(has12.error()));
  if (*has12) {
    auto value12 = config.get("fontColorDisabled");
    if (!value12)
      return std::unexpected(std::move(value12.error()));
    auto converted12 = jsonToColor(*value12);
    if (!converted12)
      return std::unexpected(std::move(converted12.error()));
    button->setFontColorDisabled(*converted12);
  }
  auto has13 = config.contains("disabled");
  if (!has13)
    return std::unexpected(std::move(has13.error()));
  if (*has13) {
    auto value13 = config.getBool("disabled");
    if (!value13)
      return std::unexpected(std::move(value13.error()));
    button->setEnabled(!*value13);
  }
  auto zlevel = config.getFloat("zlevel", 0);
  if (!zlevel)
    return std::unexpected(std::move(zlevel.error()));
  return WidgetConstructResult(button, name, *zlevel);
}

std::expected<WidgetConstructResult, StarException> WidgetParser::imageHandler(String const& name, Json const& config) {
  auto imageResult = ImageWidget::create(m_context);
  if (!imageResult)
    return std::unexpected(std::move(imageResult.error()));
  auto image = std::move(*imageResult);
  auto result1 = common(image, config);
  if (!result1)
    return std::unexpected(std::move(result1.error()));
  auto has2 = config.contains("file");
  if (!has2)
    return std::unexpected(std::move(has2.error()));
  if (*has2) {
    auto value2 = config.getString("file");
    if (!value2)
      return std::unexpected(std::move(value2.error()));
    auto result3 = image->setImage(*value2);
    if (!result3)
      return std::unexpected(std::move(result3.error()));
  }
  auto hasDrawables = config.contains("drawables");
  if (!hasDrawables)
    return std::unexpected(std::move(hasDrawables.error()));
  if (*hasDrawables) {
    auto drawableConfigs = config.getArray("drawables");
    if (!drawableConfigs)
      return std::unexpected(std::move(drawableConfigs.error()));
    auto metadata = m_context.root().imageMetadataDatabase();
    if (!metadata)
      return std::unexpected(std::move(metadata.error()));
    List<Drawable> drawables;
    for (auto const& drawableConfig : *drawableConfigs) {
      auto drawable = Drawable::create(**metadata, drawableConfig);
      if (!drawable)
        return std::unexpected(std::move(drawable.error()));
      drawables.append(std::move(*drawable));
    }
    auto result4 = image->setDrawables(std::move(drawables));
    if (!result4)
      return std::unexpected(std::move(result4.error()));
  }
  auto has5 = config.contains("scale");
  if (!has5)
    return std::unexpected(std::move(has5.error()));
  if (*has5) {
    auto value5 = config.getFloat("scale");
    if (!value5)
      return std::unexpected(std::move(value5.error()));
    auto result6 = image->setScale(*value5);
    if (!result6)
      return std::unexpected(std::move(result6.error()));
  }
  auto has7 = config.contains("rotation");
  if (!has7)
    return std::unexpected(std::move(has7.error()));
  if (*has7) {
    auto value7 = config.getFloat("rotation");
    if (!value7)
      return std::unexpected(std::move(value7.error()));
    auto result8 = image->setRotation(*value7);
    if (!result8)
      return std::unexpected(std::move(result8.error()));
  }
  auto has9 = config.contains("centered");
  if (!has9)
    return std::unexpected(std::move(has9.error()));
  if (*has9) {
    auto value9 = config.getBool("centered");
    if (!value9)
      return std::unexpected(std::move(value9.error()));
    auto result10 = image->setCentered(*value9);
    if (!result10)
      return std::unexpected(std::move(result10.error()));
  }
  auto hasTrim = config.contains("trim");
  if (!hasTrim)
    return std::unexpected(std::move(hasTrim.error()));
  if (*hasTrim) {
    auto trim = config.getBool("trim");
    if (!trim)
      return std::unexpected(std::move(trim.error()));
    auto result11 = image->setTrim(*trim);
    if (!result11)
      return std::unexpected(std::move(result11.error()));
  } else {
    auto result12 = image->setTrim(image->centered());
    if (!result12)
      return std::unexpected(std::move(result12.error()));
  }
  auto has13 = config.contains("offset");
  if (!has13)
    return std::unexpected(std::move(has13.error()));
  if (*has13) {
    auto value13 = config.get("offset");
    if (!value13)
      return std::unexpected(std::move(value13.error()));
    auto converted13 = jsonToVec2I(*value13);
    if (!converted13)
      return std::unexpected(std::move(converted13.error()));
    image->setOffset(*converted13);
  }
  auto has14 = config.contains("maxSize");
  if (!has14)
    return std::unexpected(std::move(has14.error()));
  if (*has14) {
    auto value14 = config.get("maxSize");
    if (!value14)
      return std::unexpected(std::move(value14.error()));
    auto converted14 = jsonToVec2I(*value14);
    if (!converted14)
      return std::unexpected(std::move(converted14.error()));
    auto result15 = image->setMaxSize(*converted14);
    if (!result15)
      return std::unexpected(std::move(result15.error()));
  }
  auto has16 = config.contains("minSize");
  if (!has16)
    return std::unexpected(std::move(has16.error()));
  if (*has16) {
    auto value16 = config.get("minSize");
    if (!value16)
      return std::unexpected(std::move(value16.error()));
    auto converted16 = jsonToVec2I(*value16);
    if (!converted16)
      return std::unexpected(std::move(converted16.error()));
    auto result17 = image->setMinSize(*converted16);
    if (!result17)
      return std::unexpected(std::move(result17.error()));
  }
  auto zlevel = config.getFloat("zlevel", 0);
  if (!zlevel)
    return std::unexpected(std::move(zlevel.error()));
  return WidgetConstructResult(image, name, *zlevel);
}

std::expected<WidgetConstructResult, StarException> WidgetParser::imageStretchHandler(String const& name, Json const& config) {
  auto stretchConfig = config.get("stretchSet");
  if (!stretchConfig)
    return std::unexpected(std::move(stretchConfig.error()));
  auto stretchSet = parseImageStretchSet(*stretchConfig);
  if (!stretchSet)
    return std::unexpected(std::move(stretchSet.error()));
  auto directionName = config.getString("direction", "horizontal");
  if (!directionName)
    return std::unexpected(std::move(directionName.error()));
  if (!GuiDirectionNames)
    return std::unexpected(GuiDirectionNames.error());
  auto direction = GuiDirectionNames->getLeft(*directionName);
  if (!direction)
    return std::unexpected(std::move(direction.error()));
  auto imageStretchResult = ImageStretchWidget::create(m_context, *stretchSet, direction->get());
  if (!imageStretchResult)
    return std::unexpected(std::move(imageStretchResult.error()));
  auto imageStretch = std::move(*imageStretchResult);
  auto result1 = common(imageStretch, config);
  if (!result1)
    return std::unexpected(std::move(result1.error()));
  auto zlevel = config.getFloat("zlevel", 0);
  if (!zlevel)
    return std::unexpected(std::move(zlevel.error()));
  return WidgetConstructResult(imageStretch, name, *zlevel);
}

std::expected<WidgetConstructResult, StarException> WidgetParser::spinnerHandler(String const& name, Json const& config) {
  auto container = std::make_shared<Widget>(m_context);
  auto result1 = common(container, config);
  if (!result1)
    return std::unexpected(std::move(result1.error()));
  auto callback = config.getString("callback", name);
  if (!callback)
    return std::unexpected(std::move(callback.error()));
  auto callbackDown = m_callbacks.maybeRef(*callback + ".down");
  auto callbackUp = m_callbacks.maybeRef(*callback + ".up");
  if (!callbackDown || !callbackDown->get())
    return std::unexpected(WidgetParserException::format("Failed to find spinner callback named: '{}.down'", *callback));
  if (!callbackUp || !callbackUp->get())
    return std::unexpected(WidgetParserException::format("Failed to find spinner callback named: '{}.up'", *callback));
  auto assets = m_context.root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto metadata = m_context.root().imageMetadataDatabase();
  if (!metadata)
    return std::unexpected(std::move(metadata.error()));
  auto leftBaseConfig = (*assets)->json("/interface.config:spinner.leftBase");
  if (!leftBaseConfig)
    return std::unexpected(std::move(leftBaseConfig.error()));
  auto leftBase = leftBaseConfig->toString();
  if (!leftBase)
    return std::unexpected(std::move(leftBase.error()));
  auto leftHoverConfig = (*assets)->json("/interface.config:spinner.leftHover");
  if (!leftHoverConfig)
    return std::unexpected(std::move(leftHoverConfig.error()));
  auto leftHover = leftHoverConfig->toString();
  if (!leftHover)
    return std::unexpected(std::move(leftHover.error()));
  auto rightBaseConfig = (*assets)->json("/interface.config:spinner.rightBase");
  if (!rightBaseConfig)
    return std::unexpected(std::move(rightBaseConfig.error()));
  auto rightBase = rightBaseConfig->toString();
  if (!rightBase)
    return std::unexpected(std::move(rightBase.error()));
  auto rightHoverConfig = (*assets)->json("/interface.config:spinner.rightHover");
  if (!rightHoverConfig)
    return std::unexpected(std::move(rightHoverConfig.error()));
  auto rightHover = rightHoverConfig->toString();
  if (!rightHover)
    return std::unexpected(std::move(rightHover.error()));
  auto imageSize = (*metadata)->imageSize(*leftBase);
  if (!imageSize)
    return std::unexpected(std::move(imageSize.error()));
  auto paddingConfig = (*assets)->json("/interface.config:spinner.defaultPadding");
  if (!paddingConfig)
    return std::unexpected(std::move(paddingConfig.error()));
  auto padding = paddingConfig->toInt();
  if (!padding)
    return std::unexpected(std::move(padding.error()));
  auto upOffset = config.getFloat("upOffset", static_cast<float>((*imageSize)[0]) + *padding);
  if (!upOffset)
    return std::unexpected(std::move(upOffset.error()));
  auto leftBaseImage = config.getString("leftBase", *leftBase);
  if (!leftBaseImage)
    return std::unexpected(std::move(leftBaseImage.error()));
  auto leftHoverImage = config.getString("leftHover", *leftHover);
  if (!leftHoverImage)
    return std::unexpected(std::move(leftHoverImage.error()));
  auto rightBaseImage = config.getString("rightBase", *rightBase);
  if (!rightBaseImage)
    return std::unexpected(std::move(rightBaseImage.error()));
  auto rightHoverImage = config.getString("rightHover", *rightHover);
  if (!rightHoverImage)
    return std::unexpected(std::move(rightHoverImage.error()));
  auto downResult = ButtonWidget::create(m_context, callbackDown->get(), *leftBaseImage, *leftHoverImage);
  if (!downResult)
    return std::unexpected(std::move(downResult.error()));
  auto down = std::move(*downResult);
  auto upResult = ButtonWidget::create(m_context, callbackUp->get(), *rightBaseImage, *rightHoverImage);
  if (!upResult)
    return std::unexpected(std::move(upResult.error()));
  auto up = std::move(*upResult);
  up->setPosition(up->position() + Vec2I(*upOffset, 0));
  auto result2 = container->addChild("down", down);
  if (!result2)
    return std::unexpected(std::move(result2.error()));
  auto result3 = container->addChild("up", up);
  if (!result3)
    return std::unexpected(std::move(result3.error()));
  container->disableScissoring();
  container->markAsContainer();
  container->determineSizeFromChildren();
  auto zlevel = config.getFloat("zlevel", 0);
  if (!zlevel)
    return std::unexpected(std::move(zlevel.error()));
  return WidgetConstructResult(container, name, *zlevel);
}

std::expected<WidgetConstructResult, StarException> WidgetParser::radioGroupHandler(String const& name, Json const& config) {
  auto buttonGroup = std::make_shared<ButtonGroupWidget>(m_context);
  auto result1 = common(buttonGroup, config);
  if (!result1)
    return std::unexpected(std::move(result1.error()));
  buttonGroup->markAsContainer();
  buttonGroup->disableScissoring();
  auto toggle = config.getBool("toggleMode", false);
  if (!toggle)
    return std::unexpected(std::move(toggle.error()));
  buttonGroup->setToggle(*toggle);
  auto callback = config.getString("callback", name);
  if (!callback)
    return std::unexpected(std::move(callback.error()));
  auto callbackFunc = m_callbacks.maybeRef(*callback);
  if (!callbackFunc || !callbackFunc->get())
    return std::unexpected(WidgetParserException::format("Failed to find callback named: {}", *callback));
  auto buttons = config.getArray("buttons");
  if (!buttons)
    return std::unexpected(std::move(buttons.error()));
  auto baseImage = config.getString("baseImage", "");
  if (!baseImage)
    return std::unexpected(std::move(baseImage.error()));
  auto hoverImage = config.getString("hoverImage", "");
  if (!hoverImage)
    return std::unexpected(std::move(hoverImage.error()));
  auto baseImageChecked = config.getString("baseImageChecked", "");
  if (!baseImageChecked)
    return std::unexpected(std::move(baseImageChecked.error()));
  auto hoverImageChecked = config.getString("hoverImageChecked", "");
  if (!hoverImageChecked)
    return std::unexpected(std::move(hoverImageChecked.error()));
  auto pressedImage = config.getString("pressedImage", "");
  if (!pressedImage)
    return std::unexpected(std::move(pressedImage.error()));
  auto disabledImage = config.getString("disabledImage", "");
  if (!disabledImage)
    return std::unexpected(std::move(disabledImage.error()));
  auto pressedImageChecked = config.getString("pressedImageChecked", "");
  if (!pressedImageChecked)
    return std::unexpected(std::move(pressedImageChecked.error()));
  auto disabledImageChecked = config.getString("disabledImageChecked", "");
  if (!disabledImageChecked)
    return std::unexpected(std::move(disabledImageChecked.error()));
  for (auto const& btnConfig : *buttons) {
    auto overlayImage = btnConfig.getString("image", "");
    if (!overlayImage)
      return std::unexpected(std::move(overlayImage.error()));
    auto id = btnConfig.getInt("id", ButtonGroup::NoButton);
    if (!id)
      return std::unexpected(std::move(id.error()));
    auto buttonResult = ButtonWidget::create(m_context);
    if (!buttonResult)
      return std::unexpected(std::move(buttonResult.error()));
    auto button = std::move(*buttonResult);
    button->setButtonGroup(buttonGroup, *id);
    auto buttonbaseImage = btnConfig.getString("baseImage", *baseImage);
    if (!buttonbaseImage)
      return std::unexpected(std::move(buttonbaseImage.error()));
    auto buttonhoverImage = btnConfig.getString("hoverImage", *hoverImage);
    if (!buttonhoverImage)
      return std::unexpected(std::move(buttonhoverImage.error()));
    auto buttonpressedImage = btnConfig.getString("pressedImage", *pressedImage);
    if (!buttonpressedImage)
      return std::unexpected(std::move(buttonpressedImage.error()));
    auto buttondisabledImage = btnConfig.getString("disabledImage", *disabledImage);
    if (!buttondisabledImage)
      return std::unexpected(std::move(buttondisabledImage.error()));
    auto buttonbaseImageChecked = btnConfig.getString("baseImageChecked", *baseImageChecked);
    if (!buttonbaseImageChecked)
      return std::unexpected(std::move(buttonbaseImageChecked.error()));
    auto buttonhoverImageChecked = btnConfig.getString("hoverImageChecked", *hoverImageChecked);
    if (!buttonhoverImageChecked)
      return std::unexpected(std::move(buttonhoverImageChecked.error()));
    auto buttonpressedImageChecked = btnConfig.getString("pressedImageChecked", *pressedImageChecked);
    if (!buttonpressedImageChecked)
      return std::unexpected(std::move(buttonpressedImageChecked.error()));
    auto buttondisabledImageChecked = btnConfig.getString("hoverImageChecked", *disabledImageChecked);
    if (!buttondisabledImageChecked)
      return std::unexpected(std::move(buttondisabledImageChecked.error()));
    auto result2 = button->setImages(*buttonbaseImage, *buttonhoverImage, *buttonpressedImage, *buttondisabledImage);
    if (!result2)
      return std::unexpected(std::move(result2.error()));
    auto result3 = button->setCheckedImages(*buttonbaseImageChecked, *buttonhoverImageChecked, *buttonpressedImageChecked, *buttondisabledImageChecked);
    if (!result3)
      return std::unexpected(std::move(result3.error()));
    button->setOverlayImage(*overlayImage);
    auto disabled = btnConfig.getBool("disabled", false);
    if (!disabled)
      return std::unexpected(std::move(disabled.error()));
    if (*disabled)
      button->disable();
    auto selected = btnConfig.getBool("selected", false);
    if (!selected)
      return std::unexpected(std::move(selected.error()));
    if (*selected) {
      auto result4 = button->check();
      if (!result4)
        return std::unexpected(std::move(result4.error()));
    }
    auto has5 = btnConfig.contains("fontSize");
    if (!has5)
      return std::unexpected(std::move(has5.error()));
    if (*has5) {
      auto value5 = btnConfig.getInt("fontSize");
      if (!value5)
        return std::unexpected(std::move(value5.error()));
      button->setFontSize(*value5);
    }
    auto has6 = btnConfig.contains("fontColor");
    if (!has6)
      return std::unexpected(std::move(has6.error()));
    if (*has6) {
      auto value6 = btnConfig.get("fontColor");
      if (!value6)
        return std::unexpected(std::move(value6.error()));
      auto converted6 = jsonToColor(*value6);
      if (!converted6)
        return std::unexpected(std::move(converted6.error()));
      button->setFontColor(*converted6);
    }
    auto has7 = btnConfig.contains("fontColorChecked");
    if (!has7)
      return std::unexpected(std::move(has7.error()));
    if (*has7) {
      auto value7 = btnConfig.get("fontColorChecked");
      if (!value7)
        return std::unexpected(std::move(value7.error()));
      auto converted7 = jsonToColor(*value7);
      if (!converted7)
        return std::unexpected(std::move(converted7.error()));
      button->setFontColorChecked(*converted7);
    }
    auto has8 = btnConfig.contains("fontColorDisabled");
    if (!has8)
      return std::unexpected(std::move(has8.error()));
    if (*has8) {
      auto value8 = btnConfig.get("fontColorDisabled");
      if (!value8)
        return std::unexpected(std::move(value8.error()));
      auto converted8 = jsonToColor(*value8);
      if (!converted8)
        return std::unexpected(std::move(converted8.error()));
      button->setFontColorDisabled(*converted8);
    }
    auto has9 = btnConfig.contains("text");
    if (!has9)
      return std::unexpected(std::move(has9.error()));
    if (*has9) {
      auto value9 = btnConfig.getString("text");
      if (!value9)
        return std::unexpected(std::move(value9.error()));
      button->setText(*value9);
    }
    auto has10 = btnConfig.contains("pressedOffset");
    if (!has10)
      return std::unexpected(std::move(has10.error()));
    if (*has10) {
      auto value10 = btnConfig.get("pressedOffset");
      if (!value10)
        return std::unexpected(std::move(value10.error()));
      auto converted10 = jsonToVec2I(*value10);
      if (!converted10)
        return std::unexpected(std::move(converted10.error()));
      button->setPressedOffset(*converted10);
    }
    auto result11 = common(button, btnConfig);
    if (!result11)
      return std::unexpected(std::move(result11.error()));
    auto result12 = buttonGroup->addChild(toString(button->buttonGroupId()), button);
    if (!result12)
      return std::unexpected(std::move(result12.error()));
  }
  // Install only after loading every button to avoid callbacks during parsing.
  buttonGroup->setCallback(callbackFunc->get());
  auto zlevel = config.getFloat("zlevel", 0);
  if (!zlevel)
    return std::unexpected(std::move(zlevel.error()));
  return WidgetConstructResult(buttonGroup, name, *zlevel);
}

std::expected<WidgetConstructResult, StarException> WidgetParser::portraitHandler(String const& name, Json const& config) {
  auto portraitResult = PortraitWidget::create(m_context);
  if (!portraitResult)
    return std::unexpected(std::move(portraitResult.error()));
  auto portrait = std::move(*portraitResult);
  auto hasMode = config.contains("portraitMode");
  if (!hasMode)
    return std::unexpected(std::move(hasMode.error()));
  if (*hasMode) {
    auto modeName = config.getString("portraitMode");
    if (!modeName)
      return std::unexpected(std::move(modeName.error()));
    auto mode = enumValue<PortraitModeNames>(*modeName);
    if (!mode)
      return std::unexpected(std::move(mode.error()));
    auto changedMode = portrait->setMode(*mode);
    if (!changedMode)
      return std::unexpected(std::move(changedMode.error()));
  }
  auto has1 = config.contains("renderHumanoid");
  if (!has1)
    return std::unexpected(std::move(has1.error()));
  if (*has1) {
    auto value1 = config.getBool("renderHumanoid");
    if (!value1)
      return std::unexpected(std::move(value1.error()));
    portrait->setRenderHumanoid(*value1);
  }
  auto scale = config.getFloat("scale", 1);
  if (!scale)
    return std::unexpected(std::move(scale.error()));
  auto changedScale = portrait->setScale(*scale);
  if (!changedScale)
    return std::unexpected(std::move(changedScale.error()));
  auto result2 = common(portrait, config);
  if (!result2)
    return std::unexpected(std::move(result2.error()));
  auto zlevel = config.getFloat("zlevel", 0);
  if (!zlevel)
    return std::unexpected(std::move(zlevel.error()));
  return WidgetConstructResult(portrait, name, *zlevel);
}

std::expected<WidgetConstructResult, StarException> WidgetParser::textboxHandler(String const& name, Json const& config) {
  auto callback = config.getString("callback", name);
  if (!callback)
    return std::unexpected(std::move(callback.error()));
  auto callbackFunc = m_callbacks.maybeRef(*callback);
  if (!callbackFunc || !callbackFunc->get())
    return std::unexpected(WidgetParserException::format("Failed to find callback named: {}", *callback));
  auto initialText = config.getString("value", "");
  if (!initialText)
    return std::unexpected(std::move(initialText.error()));
  auto hintText = config.getString("hint", "");
  if (!hintText)
    return std::unexpected(std::move(hintText.error()));
  auto textboxResult = TextBoxWidget::create(m_context, *initialText, *hintText, callbackFunc->get());
  if (!textboxResult)
    return std::unexpected(std::move(textboxResult.error()));
  auto textbox = std::move(*textboxResult);
  auto hasblur = config.contains("blur");
  if (!hasblur)
    return std::unexpected(std::move(hasblur.error()));
  if (*hasblur) {
    auto blurCallback = config.getString("blur");
    if (!blurCallback)
      return std::unexpected(std::move(blurCallback.error()));
    auto blurCallbackFunc = m_callbacks.maybeRef(*blurCallback);
    if (!blurCallbackFunc || !blurCallbackFunc->get())
      return std::unexpected(WidgetParserException::format("Failed to find callback named: {}", *blurCallback));
    textbox->setOnBlurCallback(blurCallbackFunc->get());
  }
  auto hasenterKey = config.contains("enterKey");
  if (!hasenterKey)
    return std::unexpected(std::move(hasenterKey.error()));
  if (*hasenterKey) {
    auto enterKeyCallback = config.getString("enterKey");
    if (!enterKeyCallback)
      return std::unexpected(std::move(enterKeyCallback.error()));
    auto enterKeyCallbackFunc = m_callbacks.maybeRef(*enterKeyCallback);
    if (!enterKeyCallbackFunc || !enterKeyCallbackFunc->get())
      return std::unexpected(WidgetParserException::format("Failed to find callback named: {}", *enterKeyCallback));
    textbox->setOnEnterKeyCallback(enterKeyCallbackFunc->get());
  }
  auto hasescapeKey = config.contains("escapeKey");
  if (!hasescapeKey)
    return std::unexpected(std::move(hasescapeKey.error()));
  if (*hasescapeKey) {
    auto escapeKeyCallback = config.getString("escapeKey");
    if (!escapeKeyCallback)
      return std::unexpected(std::move(escapeKeyCallback.error()));
    auto escapeKeyCallbackFunc = m_callbacks.maybeRef(*escapeKeyCallback);
    if (!escapeKeyCallbackFunc || !escapeKeyCallbackFunc->get())
      return std::unexpected(WidgetParserException::format("Failed to find callback named: {}", *escapeKeyCallback));
    textbox->setOnEscapeKeyCallback(escapeKeyCallbackFunc->get());
  }
  auto has1 = config.contains("nextFocus");
  if (!has1)
    return std::unexpected(std::move(has1.error()));
  if (*has1) {
    auto value1 = config.getString("nextFocus");
    if (!value1)
      return std::unexpected(std::move(value1.error()));
    textbox->setNextFocus(*value1);
  }
  auto has2 = config.contains("prevFocus");
  if (!has2)
    return std::unexpected(std::move(has2.error()));
  if (*has2) {
    auto value2 = config.getString("prevFocus");
    if (!value2)
      return std::unexpected(std::move(value2.error()));
    textbox->setPrevFocus(*value2);
  }
  auto hAnchor = config.getString("textAlign", "left");
  if (!hAnchor)
    return std::unexpected(std::move(hAnchor.error()));
  if (*hAnchor == "right")
    textbox->setTextAlign(HorizontalAnchor::RightAnchor);
  else if (*hAnchor == "center")
    textbox->setTextAlign(HorizontalAnchor::HMidAnchor);
  else if (*hAnchor != "left")
    return std::unexpected(WidgetParserException::format("Malformed gui json, expected textAlign to be left, right, or center, got {}", *hAnchor));
  auto has3 = config.contains("fontSize");
  if (!has3)
    return std::unexpected(std::move(has3.error()));
  if (*has3) {
    auto value3 = config.getInt("fontSize");
    if (!value3)
      return std::unexpected(std::move(value3.error()));
    textbox->setFontSize(*value3);
  }
  auto has4 = config.contains("color");
  if (!has4)
    return std::unexpected(std::move(has4.error()));
  if (*has4) {
    auto value4 = config.get("color");
    if (!value4)
      return std::unexpected(std::move(value4.error()));
    auto converted4 = jsonToColor(*value4);
    if (!converted4)
      return std::unexpected(std::move(converted4.error()));
    textbox->setColor(*converted4);
  }
  auto has5 = config.contains("directives");
  if (!has5)
    return std::unexpected(std::move(has5.error()));
  if (*has5) {
    auto value5 = config.getString("directives");
    if (!value5)
      return std::unexpected(std::move(value5.error()));
    textbox->setDirectives(*value5);
  }
  auto has6 = config.contains("font");
  if (!has6)
    return std::unexpected(std::move(has6.error()));
  if (*has6) {
    auto value6 = config.getString("font");
    if (!value6)
      return std::unexpected(std::move(value6.error()));
    textbox->setFont(*value6);
  }
  auto has7 = config.contains("border");
  if (!has7)
    return std::unexpected(std::move(has7.error()));
  if (*has7) {
    auto value7 = config.getBool("border");
    if (!value7)
      return std::unexpected(std::move(value7.error()));
    textbox->setDrawBorder(*value7);
  }
  auto has8 = config.contains("maxWidth");
  if (!has8)
    return std::unexpected(std::move(has8.error()));
  if (*has8) {
    auto value8 = config.getInt("maxWidth");
    if (!value8)
      return std::unexpected(std::move(value8.error()));
    textbox->setMaxWidth(*value8);
  }
  auto has9 = config.contains("regex");
  if (!has9)
    return std::unexpected(std::move(has9.error()));
  if (*has9) {
    auto value9 = config.getString("regex");
    if (!value9)
      return std::unexpected(std::move(value9.error()));
    textbox->setRegex(*value9);
  }
  auto has10 = config.contains("hidden");
  if (!has10)
    return std::unexpected(std::move(has10.error()));
  if (*has10) {
    auto value10 = config.getBool("hidden");
    if (!value10)
      return std::unexpected(std::move(value10.error()));
    textbox->setHidden(*value10);
  }
  auto result11 = common(textbox, config);
  if (!result11)
    return std::unexpected(std::move(result11.error()));
  auto zlevel = config.getFloat("zlevel", 0);
  if (!zlevel)
    return std::unexpected(std::move(zlevel.error()));
  return WidgetConstructResult(textbox, name, *zlevel);
}

std::expected<WidgetConstructResult, StarException> WidgetParser::labelHandler(String const& name, Json const& config) {
  auto text = config.getString("value", "");
  if (!text)
    return std::unexpected(std::move(text.error()));
  Color color = Color::White;
  auto has1 = config.contains("color");
  if (!has1)
    return std::unexpected(std::move(has1.error()));
  if (*has1) {
    auto value1 = config.get("color");
    if (!value1)
      return std::unexpected(std::move(value1.error()));
    auto converted1 = jsonToColor(*value1);
    if (!converted1)
      return std::unexpected(std::move(converted1.error()));
    color = *converted1;
  }
  auto horizontal = config.getString("hAnchor", "left");
  if (!horizontal)
    return std::unexpected(std::move(horizontal.error()));
  auto vertical = config.getString("vAnchor", "bottom");
  if (!vertical)
    return std::unexpected(std::move(vertical.error()));
  if (!HorizontalAnchorNames)
    return std::unexpected(HorizontalAnchorNames.error());
  auto hAnchor = HorizontalAnchorNames->getLeft(*horizontal);
  if (!hAnchor)
    return std::unexpected(std::move(hAnchor.error()));
  if (!VerticalAnchorNames)
    return std::unexpected(VerticalAnchorNames.error());
  auto vAnchor = VerticalAnchorNames->getLeft(*vertical);
  if (!vAnchor)
    return std::unexpected(std::move(vAnchor.error()));
  auto labelResult = LabelWidget::create(m_context, *text, color, hAnchor->get(), vAnchor->get());
  if (!labelResult)
    return std::unexpected(std::move(labelResult.error()));
  auto label = std::move(*labelResult);
  auto result2 = common(label, config);
  if (!result2)
    return std::unexpected(std::move(result2.error()));
  auto has3 = config.contains("fontSize");
  if (!has3)
    return std::unexpected(std::move(has3.error()));
  if (*has3) {
    auto value3 = config.getInt("fontSize");
    if (!value3)
      return std::unexpected(std::move(value3.error()));
    auto result4 = label->setFontSize(*value3);
    if (!result4)
      return std::unexpected(std::move(result4.error()));
  }
  auto has5 = config.contains("wrapWidth");
  if (!has5)
    return std::unexpected(std::move(has5.error()));
  if (*has5) {
    auto value5 = config.getInt("wrapWidth");
    if (!value5)
      return std::unexpected(std::move(value5.error()));
    auto result6 = label->setWrapWidth(*value5);
    if (!result6)
      return std::unexpected(std::move(result6.error()));
  }
  auto has7 = config.contains("charLimit");
  if (!has7)
    return std::unexpected(std::move(has7.error()));
  if (*has7) {
    auto value7 = config.getInt("charLimit");
    if (!value7)
      return std::unexpected(std::move(value7.error()));
    auto result8 = label->setTextCharLimit(*value7);
    if (!result8)
      return std::unexpected(std::move(result8.error()));
  }
  auto has9 = config.contains("lineSpacing");
  if (!has9)
    return std::unexpected(std::move(has9.error()));
  if (*has9) {
    auto value9 = config.getFloat("lineSpacing");
    if (!value9)
      return std::unexpected(std::move(value9.error()));
    auto result10 = label->setLineSpacing(*value9);
    if (!result10)
      return std::unexpected(std::move(result10.error()));
  }
  auto has11 = config.contains("directives");
  if (!has11)
    return std::unexpected(std::move(has11.error()));
  if (*has11) {
    auto value11 = config.getString("directives");
    if (!value11)
      return std::unexpected(std::move(value11.error()));
    auto result12 = label->setDirectives(*value11);
    if (!result12)
      return std::unexpected(std::move(result12.error()));
  }
  auto has13 = config.contains("font");
  if (!has13)
    return std::unexpected(std::move(has13.error()));
  if (*has13) {
    auto value13 = config.getString("font");
    if (!value13)
      return std::unexpected(std::move(value13.error()));
    auto result14 = label->setFont(*value13);
    if (!result14)
      return std::unexpected(std::move(result14.error()));
  }
  auto zlevel = config.getFloat("zlevel", 0);
  if (!zlevel)
    return std::unexpected(std::move(zlevel.error()));
  return WidgetConstructResult(label, name, *zlevel);
}

std::expected<WidgetConstructResult, StarException> WidgetParser::itemSlotHandler(String const& name, Json const& config) {
  auto backingImage = config.getString("backingImage", "");
  if (!backingImage)
    return std::unexpected(std::move(backingImage.error()));
  auto callback = config.getString("callback", name);
  if (!callback)
    return std::unexpected(std::move(callback.error()));
  auto nullCallback = callback->equals("null");
  if (!nullCallback)
    return std::unexpected(std::move(nullCallback.error()));
  String rightDefault = *nullCallback ? *callback : *callback + ".right";
  String middleDefault = *nullCallback ? *callback : *callback + ".middle";
  auto rightClickCallback = config.getString("rightClickCallback", rightDefault);
  if (!rightClickCallback)
    return std::unexpected(std::move(rightClickCallback.error()));
  auto middleClickCallback = config.getString("middleClickCallback", middleDefault);
  if (!middleClickCallback)
    return std::unexpected(std::move(middleClickCallback.error()));
  auto itemSlotResult = ItemSlotWidget::create(m_context, std::shared_ptr<Item>(), *backingImage);
  if (!itemSlotResult)
    return std::unexpected(std::move(itemSlotResult.error()));
  auto itemSlot = std::move(*itemSlotResult);
  auto leftCallback = m_callbacks.maybeRef(*callback);
  if (!leftCallback || !leftCallback->get())
    return std::unexpected(WidgetParserException::format("Failed to find itemSlot callback named: '{}'", *callback));
  itemSlot->setCallback(leftCallback->get());
  auto rightCallback = m_callbacks.maybeRef(*rightClickCallback);
  if (!rightCallback || !rightCallback->get())
    return std::unexpected(WidgetParserException::format("Failed to find itemSlot rightClickCallback named: '{}'", *rightClickCallback));
  itemSlot->setRightClickCallback(rightCallback->get());
  if (auto middleCallback = m_callbacks.maybeRef(*middleClickCallback)) {
    if (!middleCallback->get())
      return std::unexpected(WidgetParserException::format("Empty itemSlot middleClickCallback named: '{}'", *middleClickCallback));
    itemSlot->setMiddleClickCallback(middleCallback->get());
  }
  auto showFull = config.getBool("showBackingImageWhenFull", false);
  if (!showFull)
    return std::unexpected(std::move(showFull.error()));
  auto showEmpty = config.getBool("showBackingImageWhenEmpty", true);
  if (!showEmpty)
    return std::unexpected(std::move(showEmpty.error()));
  itemSlot->setBackingImageAffinity(*showFull, *showEmpty);
  auto showDurability = config.getBool("showDurability", false);
  if (!showDurability)
    return std::unexpected(std::move(showDurability.error()));
  itemSlot->showDurability(*showDurability);
  auto showCount = config.getBool("showCount", true);
  if (!showCount)
    return std::unexpected(std::move(showCount.error()));
  itemSlot->showCount(*showCount);
  auto showRarity = config.getBool("showRarity", true);
  if (!showRarity)
    return std::unexpected(std::move(showRarity.error()));
  itemSlot->showRarity(*showRarity);
  auto result1 = common(itemSlot, config);
  if (!result1)
    return std::unexpected(std::move(result1.error()));
  auto zlevel = config.getFloat("zlevel", 0);
  if (!zlevel)
    return std::unexpected(std::move(zlevel.error()));
  return WidgetConstructResult(itemSlot, name, *zlevel);
}

std::expected<WidgetConstructResult, StarException> WidgetParser::itemGridHandler(String const& name, Json const& config) {
  auto dimensionsConfig = config.get("dimensions");
  if (!dimensionsConfig)
    return std::unexpected(std::move(dimensionsConfig.error()));
  auto dimensions = jsonToVec2I(*dimensionsConfig);
  if (!dimensions)
    return std::unexpected(std::move(dimensions.error()));
  Vec2I rowSpacing;
  Vec2I columnSpacing;
  auto hasSpacing = config.contains("spacing");
  if (!hasSpacing)
    return std::unexpected(std::move(hasSpacing.error()));
  if (*hasSpacing) {
    auto spacingConfig = config.get("spacing");
    if (!spacingConfig)
      return std::unexpected(std::move(spacingConfig.error()));
    auto spacing = jsonToVec2I(*spacingConfig);
    if (!spacing)
      return std::unexpected(std::move(spacing.error()));
    rowSpacing = {(*spacing)[0], 0};
    columnSpacing = {0, (*spacing)[1]};
  } else {
    auto rowConfig = config.get("rowSpacing");
    if (!rowConfig)
      return std::unexpected(std::move(rowConfig.error()));
    auto columnConfig = config.get("columnSpacing");
    if (!columnConfig)
      return std::unexpected(std::move(columnConfig.error()));
    auto row = jsonToVec2I(*rowConfig);
    if (!row)
      return std::unexpected(std::move(row.error()));
    auto column = jsonToVec2I(*columnConfig);
    if (!column)
      return std::unexpected(std::move(column.error()));
    rowSpacing = *row;
    columnSpacing = *column;
  }
  auto callback = config.getString("callback", name);
  if (!callback)
    return std::unexpected(std::move(callback.error()));
  auto nullCallback = callback->equals("null");
  if (!nullCallback)
    return std::unexpected(std::move(nullCallback.error()));
  String rightDefault = *nullCallback ? *callback : *callback + ".right";
  String middleDefault = *nullCallback ? *callback : *callback + ".middle";
  auto rightClickCallback = config.getString("rightClickCallback", rightDefault);
  if (!rightClickCallback)
    return std::unexpected(std::move(rightClickCallback.error()));
  auto middleClickCallback = config.getString("middleClickCallback", middleDefault);
  if (!middleClickCallback)
    return std::unexpected(std::move(middleClickCallback.error()));
  auto slotOffset = config.getInt("slotOffset", 0);
  if (!slotOffset)
    return std::unexpected(std::move(slotOffset.error()));
  auto backingImage = config.getString("backingImage", "");
  if (!backingImage)
    return std::unexpected(std::move(backingImage.error()));
  auto itemGridResult = ItemGridWidget::create(m_context, std::shared_ptr<ItemBag const>(), *dimensions, rowSpacing, columnSpacing, *backingImage, *slotOffset);
  if (!itemGridResult)
    return std::unexpected(std::move(itemGridResult.error()));
  auto itemGrid = std::move(*itemGridResult);
  auto showFull = config.getBool("showBackingImageWhenFull", false);
  if (!showFull)
    return std::unexpected(std::move(showFull.error()));
  auto showEmpty = config.getBool("showBackingImageWhenEmpty", true);
  if (!showEmpty)
    return std::unexpected(std::move(showEmpty.error()));
  auto backingAffinity = itemGrid->setBackingImageAffinity(*showFull, *showEmpty);
  if (!backingAffinity)
    return std::unexpected(std::move(backingAffinity.error()));
  auto durability = config.getBool("showDurability", false);
  if (!durability)
    return std::unexpected(std::move(durability.error()));
  auto showDurability = itemGrid->showDurability(*durability);
  if (!showDurability)
    return std::unexpected(std::move(showDurability.error()));
  auto leftCallback = m_callbacks.maybeRef(*callback);
  if (!leftCallback || !leftCallback->get())
    return std::unexpected(WidgetParserException::format("Failed to find itemGrid callback named: '{}'", *callback));
  itemGrid->setCallback(leftCallback->get());
  auto rightCallback = m_callbacks.maybeRef(*rightClickCallback);
  if (!rightCallback || !rightCallback->get())
    return std::unexpected(WidgetParserException::format("Failed to find itemGrid rightClickCallback named: '{}'", *rightClickCallback));
  itemGrid->setRightClickCallback(rightCallback->get());
  if (auto middleCallback = m_callbacks.maybeRef(*middleClickCallback)) {
    if (!middleCallback->get())
      return std::unexpected(WidgetParserException::format("Empty itemGrid middleClickCallback named: '{}'", *middleClickCallback));
    itemGrid->setMiddleClickCallback(middleCallback->get());
  }
  auto result1 = common(itemGrid, config);
  if (!result1)
    return std::unexpected(std::move(result1.error()));
  auto zlevel = config.getFloat("zlevel", 0);
  if (!zlevel)
    return std::unexpected(std::move(zlevel.error()));
  return WidgetConstructResult(itemGrid, name, *zlevel);
}

std::expected<WidgetConstructResult, StarException> WidgetParser::listHandler(String const& name, Json const& config) {
  auto schema = config.get("schema");
  if (!schema)
    return std::unexpected(std::move(schema.error()));
  auto listResult = ListWidget::create(m_context, *schema);
  if (!listResult)
    return std::unexpected(std::move(listResult.error()));
  auto list = std::move(*listResult);
  auto result1 = common(list, config);
  if (!result1)
    return std::unexpected(std::move(result1.error()));
  auto callback = config.getString("callback", name);
  if (!callback)
    return std::unexpected(std::move(callback.error()));
  auto callbackFunc = m_callbacks.maybeRef(*callback);
  if (callbackFunc && callbackFunc->get())
    list->setCallback(callbackFunc->get());
  auto fillDown = config.getBool("fillDown", false);
  if (!fillDown)
    return std::unexpected(std::move(fillDown.error()));
  list->setFillDown(*fillDown);
  auto columns = config.getUInt("columns", 1);
  if (!columns)
    return std::unexpected(std::move(columns.error()));
  auto result2 = list->setColumns(*columns);
  if (!result2)
    return std::unexpected(std::move(result2.error()));
  auto zlevel = config.getFloat("zlevel", 0);
  if (!zlevel)
    return std::unexpected(std::move(zlevel.error()));
  return WidgetConstructResult(list, name, *zlevel);
}

std::expected<WidgetConstructResult, StarException> WidgetParser::sliderHandler(String const& name, Json const& config) {
  auto grid = config.getString("gridImage");
  if (!grid)
    return std::unexpected(std::move(grid.error()));
  auto showSpinner = config.getBool("showSpinner", true);
  if (!showSpinner)
    return std::unexpected(std::move(showSpinner.error()));
  auto sliderResult = SliderBarWidget::create(m_context, *grid, *showSpinner);
  if (!sliderResult)
    return std::unexpected(std::move(sliderResult.error()));
  auto slider = std::move(*sliderResult);
  auto result1 = common(slider, config);
  if (!result1)
    return std::unexpected(std::move(result1.error()));
  auto callback = config.getString("callback", name);
  if (!callback)
    return std::unexpected(std::move(callback.error()));
  auto callbackFunc = m_callbacks.maybeRef(*callback);
  if (callbackFunc && callbackFunc->get())
    slider->setCallback(callbackFunc->get());
  auto hasRange = config.contains("range");
  if (!hasRange)
    return std::unexpected(std::move(hasRange.error()));
  if (*hasRange) {
    auto range = config.get("range");
    if (!range)
      return std::unexpected(std::move(range.error()));
    auto range0 = range->getInt(0);
    if (!range0)
      return std::unexpected(std::move(range0.error()));
    auto range1 = range->getInt(1);
    if (!range1)
      return std::unexpected(std::move(range1.error()));
    auto range2 = range->getInt(2);
    if (!range2)
      return std::unexpected(std::move(range2.error()));
    auto result2 = slider->setRange(*range0, *range1, *range2);
    if (!result2)
      return std::unexpected(std::move(result2.error()));
  }
  auto hasJog = config.contains("jogImages");
  if (!hasJog)
    return std::unexpected(std::move(hasJog.error()));
  if (*hasJog) {
    auto jogConfig = config.get("jogImages");
    if (!jogConfig)
      return std::unexpected(std::move(jogConfig.error()));
    auto baseImage = (*jogConfig).getString("baseImage");
    if (!baseImage)
      return std::unexpected(std::move(baseImage.error()));
    auto hoverImage = (*jogConfig).getString("hoverImage", "");
    if (!hoverImage)
      return std::unexpected(std::move(hoverImage.error()));
    auto pressedImage = (*jogConfig).getString("pressedImage", "");
    if (!pressedImage)
      return std::unexpected(std::move(pressedImage.error()));
    auto disabledImage = (*jogConfig).getString("disabledImage", "");
    if (!disabledImage)
      return std::unexpected(std::move(disabledImage.error()));
    auto result3 = slider->setJogImages(*baseImage, *hoverImage, *pressedImage, *disabledImage);
    if (!result3)
      return std::unexpected(std::move(result3.error()));
  }
  auto has4 = config.contains("disabled");
  if (!has4)
    return std::unexpected(std::move(has4.error()));
  if (*has4) {
    auto value4 = config.getBool("disabled");
    if (!value4)
      return std::unexpected(std::move(value4.error()));
    slider->setEnabled(!*value4);
  }
  auto zlevel = config.getFloat("zlevel", 0);
  if (!zlevel)
    return std::unexpected(std::move(zlevel.error()));
  return WidgetConstructResult(slider, name, *zlevel);
}

std::expected<WidgetConstructResult, StarException> WidgetParser::largeCharPlateHandler(String const& name, Json const& config) {
  auto callback = config.getString("callback", name);
  if (!callback)
    return std::unexpected(std::move(callback.error()));
  auto callbackFunc = m_callbacks.maybeRef(*callback);
  if (!callbackFunc || !callbackFunc->get())
    return std::unexpected(WidgetParserException::format("Failed to find callback named: {}", *callback));
  auto charPlateResult = LargeCharPlateWidget::create(m_context, callbackFunc->get());
  if (!charPlateResult)
    return std::unexpected(std::move(charPlateResult.error()));
  auto charPlate = std::move(*charPlateResult);
  auto result1 = common(charPlate, config);
  if (!result1)
    return std::unexpected(std::move(result1.error()));
  auto zlevel = config.getFloat("zlevel", 0);
  if (!zlevel)
    return std::unexpected(std::move(zlevel.error()));
  return WidgetConstructResult(charPlate, name, *zlevel);
}

std::expected<WidgetConstructResult, StarException> WidgetParser::tabSetHandler(String const& name, Json const& config) {
  TabSetConfig tabSetConfig;
  auto tabButtonBaseImage = config.getString("tabButtonBaseImage");
  if (!tabButtonBaseImage)
    return std::unexpected(std::move(tabButtonBaseImage.error()));
  tabSetConfig.tabButtonBaseImage = std::move(*tabButtonBaseImage);
  auto tabButtonHoverImage = config.getString("tabButtonHoverImage");
  if (!tabButtonHoverImage)
    return std::unexpected(std::move(tabButtonHoverImage.error()));
  tabSetConfig.tabButtonHoverImage = std::move(*tabButtonHoverImage);
  auto tabButtonPressedImage = config.getString("tabButtonPressedImage", tabSetConfig.tabButtonHoverImage);
  if (!tabButtonPressedImage)
    return std::unexpected(std::move(tabButtonPressedImage.error()));
  tabSetConfig.tabButtonPressedImage = std::move(*tabButtonPressedImage);
  auto tabButtonBaseImageSelected = config.getString("tabButtonBaseImageSelected", tabSetConfig.tabButtonBaseImage);
  if (!tabButtonBaseImageSelected)
    return std::unexpected(std::move(tabButtonBaseImageSelected.error()));
  tabSetConfig.tabButtonBaseImageSelected = std::move(*tabButtonBaseImageSelected);
  auto tabButtonHoverImageSelected = config.getString("tabButtonHoverImageSelected", tabSetConfig.tabButtonHoverImage);
  if (!tabButtonHoverImageSelected)
    return std::unexpected(std::move(tabButtonHoverImageSelected.error()));
  tabSetConfig.tabButtonHoverImageSelected = std::move(*tabButtonHoverImageSelected);
  auto tabButtonPressedImageSelected = config.getString("tabButtonPressedImageSelected", tabSetConfig.tabButtonHoverImageSelected);
  if (!tabButtonPressedImageSelected)
    return std::unexpected(std::move(tabButtonPressedImageSelected.error()));
  tabSetConfig.tabButtonPressedImageSelected = std::move(*tabButtonPressedImageSelected);
  auto assets = m_context.root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto defaultPressedOffset = (*assets)->json("/interface.config:buttonPressedOffset");
  if (!defaultPressedOffset)
    return std::unexpected(std::move(defaultPressedOffset.error()));
  auto pressedOffsetConfig = config.get("tabButtonPressedOffset", *defaultPressedOffset);
  if (!pressedOffsetConfig)
    return std::unexpected(std::move(pressedOffsetConfig.error()));
  auto pressedOffset = jsonToVec2I(*pressedOffsetConfig);
  if (!pressedOffset)
    return std::unexpected(std::move(pressedOffset.error()));
  tabSetConfig.tabButtonPressedOffset = *pressedOffset;
  auto tabButtonTextOffset = config.opt("tabButtonTextOffset");
  if (!tabButtonTextOffset)
    return std::unexpected(std::move(tabButtonTextOffset.error()));
  if (*tabButtonTextOffset) {
    auto tabButtonTextOffsetVector = jsonToVec2I(**tabButtonTextOffset);
    if (!tabButtonTextOffsetVector)
      return std::unexpected(std::move(tabButtonTextOffsetVector.error()));
    tabSetConfig.tabButtonTextOffset = *tabButtonTextOffsetVector;
  } else {
    tabSetConfig.tabButtonTextOffset = Vec2I{};
  }
  auto tabButtonSpacing = config.opt("tabButtonSpacing");
  if (!tabButtonSpacing)
    return std::unexpected(std::move(tabButtonSpacing.error()));
  if (*tabButtonSpacing) {
    auto tabButtonSpacingVector = jsonToVec2I(**tabButtonSpacing);
    if (!tabButtonSpacingVector)
      return std::unexpected(std::move(tabButtonSpacingVector.error()));
    tabSetConfig.tabButtonSpacing = *tabButtonSpacingVector;
  } else {
    tabSetConfig.tabButtonSpacing = Vec2I{};
  }
  auto tabSetResult = TabSetWidget::create(m_context, tabSetConfig);
  if (!tabSetResult)
    return std::unexpected(std::move(tabSetResult.error()));
  auto tabSet = std::move(*tabSetResult);
  auto result1 = common(tabSet, config);
  if (!result1)
    return std::unexpected(std::move(result1.error()));
  auto tabs = config.getArray("tabs");
  if (!tabs)
    return std::unexpected(std::move(tabs.error()));
  for (auto const& entry : *tabs) {
    auto widget = std::make_shared<Widget>(m_context);
    auto children = entry.get("children");
    if (!children)
      return std::unexpected(std::move(children.error()));
    auto result2 = constructImpl(*children, *widget);
    if (!result2)
      return std::unexpected(std::move(result2.error()));
    widget->determineSizeFromChildren();
    auto tabName = entry.getString("tabName");
    if (!tabName)
      return std::unexpected(std::move(tabName.error()));
    auto tabTitle = entry.getString("tabTitle");
    if (!tabTitle)
      return std::unexpected(std::move(tabTitle.error()));
    auto result3 = tabSet->addTab(*tabName, widget, *tabTitle);
    if (!result3)
      return std::unexpected(std::move(result3.error()));
  }
  auto zlevel = config.getFloat("zlevel", 0);
  if (!zlevel)
    return std::unexpected(std::move(zlevel.error()));
  return WidgetConstructResult(tabSet, name, *zlevel);
}

std::expected<WidgetConstructResult, StarException> WidgetParser::widgetHandler(String const& name, Json const& config) {
  auto widget = std::make_shared<Widget>(m_context);
  auto result1 = common(widget, config);
  if (!result1)
    return std::unexpected(std::move(result1.error()));
  auto zlevel = config.getFloat("zlevel", 0);
  if (!zlevel)
    return std::unexpected(std::move(zlevel.error()));
  return WidgetConstructResult(widget, name, *zlevel);
}

std::expected<WidgetConstructResult, StarException> WidgetParser::containerHandler(String const& name, Json const& config) {
  auto widget = widgetHandler(name, config);
  if (!widget)
    return std::unexpected(std::move(widget.error()));
  widget->obj->disableScissoring();
  widget->obj->markAsContainer();
  return std::move(*widget);
}

std::expected<WidgetConstructResult, StarException> WidgetParser::layoutHandler(String const& name, Json const& config) {
  auto type = config.getString("layoutType");
  if (!type)
    return std::unexpected(std::move(type.error()));
  std::shared_ptr<Widget> widget;
  if (*type == "flow") {
    auto flow = std::make_shared<FlowLayout>(m_context);
    auto spacingConfig = config.get("spacing");
    if (!spacingConfig)
      return std::unexpected(std::move(spacingConfig.error()));
    auto spacing = jsonToVec2I(*spacingConfig);
    if (!spacing)
      return std::unexpected(std::move(spacing.error()));
    flow->setSpacing(*spacing);
    widget = std::move(flow);
  } else if (*type == "vertical") {
    auto vert = std::make_shared<VerticalLayout>(m_context);
    auto hName = config.getString("hAnchor", "left");
    if (!hName)
      return std::unexpected(std::move(hName.error()));
    auto vName = config.getString("vAnchor", "top");
    if (!vName)
      return std::unexpected(std::move(vName.error()));
    if (!HorizontalAnchorNames)
      return std::unexpected(HorizontalAnchorNames.error());
    auto hAnchor = HorizontalAnchorNames->getLeft(*hName);
    if (!hAnchor)
      return std::unexpected(std::move(hAnchor.error()));
    if (!VerticalAnchorNames)
      return std::unexpected(VerticalAnchorNames.error());
    auto vAnchor = VerticalAnchorNames->getLeft(*vName);
    if (!vAnchor)
      return std::unexpected(std::move(vAnchor.error()));
    vert->setHorizontalAnchor(hAnchor->get());
    vert->setVerticalAnchor(vAnchor->get());
    auto spacing = config.getInt("spacing", 0);
    if (!spacing)
      return std::unexpected(std::move(spacing.error()));
    auto fillDown = config.getBool("fillDown", false);
    if (!fillDown)
      return std::unexpected(std::move(fillDown.error()));
    vert->setVerticalSpacing(*spacing);
    vert->setFillDown(*fillDown);
    widget = std::move(vert);
  } else if (*type == "basic") {
    widget = std::make_shared<Layout>(m_context);
  } else {
    return std::unexpected(WidgetParserException::format("Invalid layout type {}. Options are basic, flow, vertical.", *type));
  }
  auto result1 = common(widget, config);
  if (!result1)
    return std::unexpected(std::move(result1.error()));
  auto result2 = widget->update(0);
  if (!result2)
    return std::unexpected(std::move(result2.error()));
  auto zlevel = config.getFloat("zlevel", 0);
  if (!zlevel)
    return std::unexpected(std::move(zlevel.error()));
  return WidgetConstructResult(widget, name, *zlevel);
}

std::expected<WidgetConstructResult, StarException> WidgetParser::canvasHandler(String const& name, Json const& config) {
  auto canvasResult = CanvasWidget::create(m_context);
  if (!canvasResult)
    return std::unexpected(std::move(canvasResult.error()));
  auto canvas = std::move(*canvasResult);
  auto keyboard = config.getBool("captureKeyboardEvents", false);
  if (!keyboard)
    return std::unexpected(std::move(keyboard.error()));
  auto mouse = config.getBool("captureMouseEvents", false);
  if (!mouse)
    return std::unexpected(std::move(mouse.error()));
  canvas->setCaptureKeyboardEvents(*keyboard);
  canvas->setCaptureMouseEvents(*mouse);
  auto ignoreInterfaceScale = config.getBool("ignoreInterfaceScale", false);
  if (!ignoreInterfaceScale)
    return std::unexpected(std::move(ignoreInterfaceScale.error()));
  canvas->setIgnoreInterfaceScale(*ignoreInterfaceScale);
  auto result1 = common(canvas, config);
  if (!result1)
    return std::unexpected(std::move(result1.error()));
  auto zlevel = config.getFloat("zlevel", 0);
  if (!zlevel)
    return std::unexpected(std::move(zlevel.error()));
  return WidgetConstructResult(canvas, name, *zlevel);
}

std::expected<WidgetConstructResult, StarException> WidgetParser::fuelGaugeHandler(String const& name, Json const& config) {
  auto fuelGaugeResult = FuelWidget::create(m_context);
  if (!fuelGaugeResult)
    return std::unexpected(std::move(fuelGaugeResult.error()));
  auto fuelGauge = std::move(*fuelGaugeResult);
  auto result1 = common(fuelGauge, config);
  if (!result1)
    return std::unexpected(std::move(result1.error()));
  auto zlevel = config.getFloat("zlevel", 0);
  if (!zlevel)
    return std::unexpected(std::move(zlevel.error()));
  return WidgetConstructResult(fuelGauge, name, *zlevel);
}

std::expected<WidgetConstructResult, StarException> WidgetParser::progressHandler(String const& name, Json const& config) {
  auto backgroundConfig = config.get("background", "");
  if (!backgroundConfig)
    return std::unexpected(std::move(backgroundConfig.error()));
  auto background = backgroundConfig->toString();
  if (!background)
    return std::unexpected(std::move(background.error()));
  auto overlayConfig = config.get("overlay", "");
  if (!overlayConfig)
    return std::unexpected(std::move(overlayConfig.error()));
  auto overlay = overlayConfig->toString();
  if (!overlay)
    return std::unexpected(std::move(overlay.error()));
  auto progressConfig = config.get("progressSet");
  if (!progressConfig)
    return std::unexpected(std::move(progressConfig.error()));
  auto progressSet = parseImageStretchSet(*progressConfig);
  if (!progressSet)
    return std::unexpected(std::move(progressSet.error()));
  auto directionName = config.getString("direction", "horizontal");
  if (!directionName)
    return std::unexpected(std::move(directionName.error()));
  if (!GuiDirectionNames)
    return std::unexpected(GuiDirectionNames.error());
  auto direction = GuiDirectionNames->getLeft(*directionName);
  if (!direction)
    return std::unexpected(std::move(direction.error()));
  auto progressResult = ProgressWidget::create(m_context, *background, *overlay, *progressSet, direction->get());
  if (!progressResult)
    return std::unexpected(std::move(progressResult.error()));
  auto progress = std::move(*progressResult);
  auto result1 = common(progress, config);
  if (!result1)
    return std::unexpected(std::move(result1.error()));
  auto has2 = config.contains("barColor");
  if (!has2)
    return std::unexpected(std::move(has2.error()));
  if (*has2) {
    auto value2 = config.get("barColor");
    if (!value2)
      return std::unexpected(std::move(value2.error()));
    auto converted2 = jsonToColor(*value2);
    if (!converted2)
      return std::unexpected(std::move(converted2.error()));
    progress->setColor(*converted2);
  }
  auto has3 = config.contains("max");
  if (!has3)
    return std::unexpected(std::move(has3.error()));
  if (*has3) {
    auto value3 = config.getFloat("max");
    if (!value3)
      return std::unexpected(std::move(value3.error()));
    progress->setMaxProgressLevel(*value3);
  }
  auto has4 = config.contains("initial");
  if (!has4)
    return std::unexpected(std::move(has4.error()));
  if (*has4) {
    auto value4 = config.getFloat("initial");
    if (!value4)
      return std::unexpected(std::move(value4.error()));
    progress->setCurrentProgressLevel(*value4);
  }
  auto zlevel = config.getFloat("zlevel", 0);
  if (!zlevel)
    return std::unexpected(std::move(zlevel.error()));
  return WidgetConstructResult(progress, name, *zlevel);
}

std::expected<WidgetConstructResult, StarException> WidgetParser::stackHandler(String const& name, Json const& config) {
  auto stack = std::make_shared<StackWidget>(m_context);
  auto hasStack = config.contains("stack");
  if (!hasStack)
    return std::unexpected(std::move(hasStack.error()));
  if (*hasStack) {
    auto stackList = config.getArray("stack");
    if (!stackList)
      return std::unexpected(std::move(stackList.error()));
    for (auto const& widgetCfg : *stackList) {
      auto widget = std::make_shared<Widget>(m_context);
      auto result1 = constructImpl(widgetCfg, *widget);
      if (!result1)
        return std::unexpected(std::move(result1.error()));
      widget->determineSizeFromChildren();
      auto result2 = stack->addChild(toString(stack->numChildren()), widget);
      if (!result2)
        return std::unexpected(std::move(result2.error()));
    }
  }
  stack->determineSizeFromChildren();
  auto result3 = common(stack, config);
  if (!result3)
    return std::unexpected(std::move(result3.error()));
  auto zlevel = config.getFloat("zlevel", 0);
  if (!zlevel)
    return std::unexpected(std::move(zlevel.error()));
  return WidgetConstructResult(stack, name, *zlevel);
}

std::expected<WidgetConstructResult, StarException> WidgetParser::scrollAreaHandler(String const& name, Json const& config) {
  auto scrollAreaResult = ScrollArea::create(m_context);
  if (!scrollAreaResult)
    return std::unexpected(std::move(scrollAreaResult.error()));
  auto scrollArea = std::move(*scrollAreaResult);
  auto has1 = config.contains("buttons");
  if (!has1)
    return std::unexpected(std::move(has1.error()));
  if (*has1) {
    auto value1 = config.get("buttons");
    if (!value1)
      return std::unexpected(std::move(value1.error()));
    auto result2 = scrollArea->setButtonImages(*value1);
    if (!result2)
      return std::unexpected(std::move(result2.error()));
  }
  auto has3 = config.contains("thumbs");
  if (!has3)
    return std::unexpected(std::move(has3.error()));
  if (*has3) {
    auto value3 = config.get("thumbs");
    if (!value3)
      return std::unexpected(std::move(value3.error()));
    auto result4 = scrollArea->setThumbImages(*value3);
    if (!result4)
      return std::unexpected(std::move(result4.error()));
  }
  auto has5 = config.contains("children");
  if (!has5)
    return std::unexpected(std::move(has5.error()));
  if (*has5) {
    auto value5 = config.get("children");
    if (!value5)
      return std::unexpected(std::move(value5.error()));
    auto result6 = constructImpl(*value5, *scrollArea);
    if (!result6)
      return std::unexpected(std::move(result6.error()));
  }
  auto has7 = config.contains("horizontalScroll");
  if (!has7)
    return std::unexpected(std::move(has7.error()));
  if (*has7) {
    auto value7 = config.getBool("horizontalScroll");
    if (!value7)
      return std::unexpected(std::move(value7.error()));
    scrollArea->setHorizontalScroll(*value7);
  }
  auto has8 = config.contains("verticalScroll");
  if (!has8)
    return std::unexpected(std::move(has8.error()));
  if (*has8) {
    auto value8 = config.getBool("verticalScroll");
    if (!value8)
      return std::unexpected(std::move(value8.error()));
    scrollArea->setVerticalScroll(*value8);
  }
  auto updatesChildren = config.getBool("updatesChildren", false);
  if (!updatesChildren)
    return std::unexpected(std::move(updatesChildren.error()));
  scrollArea->setUpdatesChildren(*updatesChildren);
  auto result9 = common(scrollArea, config, false);
  if (!result9)
    return std::unexpected(std::move(result9.error()));
  auto zlevel = config.getFloat("zlevel", 0);
  if (!zlevel)
    return std::unexpected(std::move(zlevel.error()));
  return WidgetConstructResult(scrollArea, name, *zlevel);
}

std::expected<void, StarException> WidgetParser::common(std::shared_ptr<Widget> widget, Json const& config, bool getChildren) {
  auto hasRect = config.contains("rect");
  if (!hasRect)
    return std::unexpected(std::move(hasRect.error()));
  if (*hasRect) {
    auto rectConfig = config.get("rect");
    if (!rectConfig)
      return std::unexpected(std::move(rectConfig.error()));
    auto rect = jsonToRectI(*rectConfig);
    if (!rect)
      return std::unexpected(std::move(rect.error()));
    widget->setPosition(rect->min());
    widget->setSize(rect->size());
  } else {
    auto has1 = config.contains("size");
    if (!has1)
      return std::unexpected(std::move(has1.error()));
    if (*has1) {
      auto value1 = config.get("size");
      if (!value1)
        return std::unexpected(std::move(value1.error()));
      auto converted1 = jsonToVec2I(*value1);
      if (!converted1)
        return std::unexpected(std::move(converted1.error()));
      widget->setSize(*converted1);
    }
    auto has2 = config.contains("position");
    if (!has2)
      return std::unexpected(std::move(has2.error()));
    if (*has2) {
      auto value2 = config.get("position");
      if (!value2)
        return std::unexpected(std::move(value2.error()));
      auto converted2 = jsonToVec2I(*value2);
      if (!converted2)
        return std::unexpected(std::move(converted2.error()));
      widget->setPosition(*converted2);
    }
  }
  auto has3 = config.contains("visible");
  if (!has3)
    return std::unexpected(std::move(has3.error()));
  if (*has3) {
    auto value3 = config.getBool("visible");
    if (!value3)
      return std::unexpected(std::move(value3.error()));
    auto result4 = widget->setVisibility(*value3);
    if (!result4)
      return std::unexpected(std::move(result4.error()));
  }
  auto focus = config.getBool("focus", false);
  if (!focus)
    return std::unexpected(std::move(focus.error()));
  if (*focus) {
    auto result5 = widget->focus();
    if (!result5)
      return std::unexpected(std::move(result5.error()));
  }
  auto has6 = config.contains("data");
  if (!has6)
    return std::unexpected(std::move(has6.error()));
  if (*has6) {
    auto value6 = config.get("data");
    if (!value6)
      return std::unexpected(std::move(value6.error()));
    widget->setData(*value6);
  }
  auto scissoring = config.getBool("scissoring", true);
  if (!scissoring)
    return std::unexpected(std::move(scissoring.error()));
  if (!*scissoring)
    widget->disableScissoring();
  auto mouseTransparent = config.getBool("mouseTransparent", false);
  if (!mouseTransparent)
    return std::unexpected(std::move(mouseTransparent.error()));
  widget->setMouseTransparent(*mouseTransparent);
  if (getChildren) {
    auto has7 = config.contains("children");
    if (!has7)
      return std::unexpected(std::move(has7.error()));
    if (*has7) {
      auto value7 = config.get("children");
      if (!value7)
        return std::unexpected(std::move(value7.error()));
      auto result8 = constructImpl(*value7, *widget);
      if (!result8)
        return std::unexpected(std::move(result8.error()));
    }
  }
  return {};
}

std::expected<ImageStretchSet, StarException> WidgetParser::parseImageStretchSet(Json const& config) {
  ImageStretchSet res;
  auto beginConfig = config.get("begin", "");
  if (!beginConfig)
    return std::unexpected(std::move(beginConfig.error()));
  auto begin = beginConfig->toString();
  if (!begin)
    return std::unexpected(std::move(begin.error()));
  res.begin = std::move(*begin);
  auto innerConfig = config.get("inner", "");
  if (!innerConfig)
    return std::unexpected(std::move(innerConfig.error()));
  auto inner = innerConfig->toString();
  if (!inner)
    return std::unexpected(std::move(inner.error()));
  res.inner = std::move(*inner);
  auto endConfig = config.get("end", "");
  if (!endConfig)
    return std::unexpected(std::move(endConfig.error()));
  auto end = endConfig->toString();
  if (!end)
    return std::unexpected(std::move(end.error()));
  res.end = std::move(*end);
  auto typeConfig = config.get("type", "stretch");
  if (!typeConfig)
    return std::unexpected(std::move(typeConfig.error()));
  auto type = typeConfig->toString();
  if (!type)
    return std::unexpected(std::move(type.error()));
  if (*type == "repeat")
    res.type = ImageStretchSet::ImageStretchType::Repeat;
  else if (*type == "stretch")
    res.type = ImageStretchSet::ImageStretchType::Stretch;
  else
    return std::unexpected(WidgetParserException::format("Could not parse Image Stretch Set, unknown type: {}", *type));
  return res;
}

}
