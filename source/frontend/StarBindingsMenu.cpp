import star.lua_root;
import std;
import star.format;
import star.json;
import star.lua;
import star.lua_converters;
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
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.string;
import star.text;
import star.asset_path;
import star.listener;
import star.game_types;
import star.set;
import star.audio;
import star.map;



import star.application;
import star.statistics_service;
import star.p2p_networking_service;
import star.user_generated_content_service;
import star.desktop_service;
import star.image;
import star.application_controller;
import star.renderer;
import star.lua_components;
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
import star.widget_parsing;
import star.gui_reader;


import star.base_script_pane;


import star.bindings_menu;

import star.input_lua_bindings;

namespace Star {
namespace {

std::expected<void, StarException> setGameBindings(GuiContext& context, Json const& bindings) {
  // The runtime loader skips malformed descriptors in existing configs. An editor
  // write must reject the whole change instead of saving a partly usable map.
  if (!InterfaceActionNames)
    return std::unexpected(InterfaceActionNames.error());
  auto actions = bindings.iterate_object();
  if (!actions)
    return std::unexpected(std::move(actions.error()));
  for (auto const& [name, inputs] : *actions) {
    if (auto action = InterfaceActionNames->get_left(name); !action)
      return std::unexpected(std::move(action.error()));
    auto descriptors = inputs.iterate_array();
    if (!descriptors)
      return std::unexpected(descriptors.error().with_context(Star::format("Bindings for {}", name)));
    for (auto const& descriptor : *descriptors) {
      auto chord = inputDescriptorFromJson(descriptor);
      if (!chord)
        return std::unexpected(chord.error().with_context(Star::format("Binding for {}", name)));
      // Legacy numeric key codes are accepted, but must not narrow or truncate.
      auto value = descriptor.get("value");
      if (!value)
        return std::unexpected(std::move(value.error()));
      if (!value->is_type(Json::Type::String)) {
        auto number = value->to_double();
        if (!number)
          return std::unexpected(std::move(number.error()));
        if (*number != std::to_underlying(chord->key) || chord->key > key_names.back().first)
          return std::unexpected(StarException::format("Invalid numeric key {} for {}", *number, name));
      }
    }
  }
  auto config = context.root().configuration();
  if (!config)
    return std::unexpected(std::move(config.error()));
  if (auto result = (*config)->set("bindings", bindings); !result)
    return std::unexpected(std::move(result.error()));
  return context.refreshKeybindings();
}

}

BindingsMenu::BindingsMenu(GuiContext& guiContext) : BaseScriptPane(guiContext) {}

std::expected<std::shared_ptr<BindingsMenu>, StarException> BindingsMenu::create(GuiContext& guiContext, Json const& config) {
  auto menu = std::shared_ptr<BindingsMenu>(new BindingsMenu(guiContext));
  if (auto result = menu->init(config); !result)
    return std::unexpected(std::move(result.error()));
  auto luaRoot = LuaRoot::create(guiContext.root());
  if (!luaRoot)
    return std::unexpected(std::move(luaRoot.error()));
  if (auto result = menu->set_lua_root(std::move(*luaRoot)); !result)
    return std::unexpected(std::move(result.error()));
  auto input = guiContext.input();
  if (!input)
    return std::unexpected(std::move(input.error()));
  auto callbacks = LuaBindings::make_input_callbacks(input->get());
  if (!callbacks)
    return std::unexpected(std::move(callbacks.error()));
  if (auto result = menu->add_callbacks("input", std::move(*callbacks)); !result)
    return std::unexpected(std::move(result.error()));

  LuaCallbacks gameBinds;
  if (auto result = gameBinds.register_callback("get", [&guiContext] {
        return guiContext.root().configuration().and_then([](auto const& config) { return config->get("bindings"); });
      }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = gameBinds.register_callback("defaults", [&guiContext] {
        return guiContext.root().configuration().and_then([](std::shared_ptr<Configuration> const& config) {
          return config->get_default("bindings");
        });
      }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = gameBinds.register_callback("set", [&guiContext](Json const& bindings) {
        return setGameBindings(guiContext, bindings);
      }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = menu->add_callbacks("gameBinds", std::move(gameBinds)); !result)
    return std::unexpected(std::move(result.error()));
  return menu;
}

}