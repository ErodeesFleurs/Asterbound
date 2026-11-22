import star.lua_root;
import std;
import star.json;
import star.format;
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


import star.voice_settings_menu;

import star.voice_lua_bindings;

namespace Star {

VoiceSettingsMenu::VoiceSettingsMenu(GuiContext& guiContext) : BaseScriptPane(guiContext) {}

std::expected<std::shared_ptr<VoiceSettingsMenu>, StarException> VoiceSettingsMenu::create(GuiContext& guiContext, Json const& config) {
  auto menu = std::shared_ptr<VoiceSettingsMenu>(new VoiceSettingsMenu(guiContext));
  if (auto initialized = menu->init(config); !initialized)
    return std::unexpected(std::move(initialized.error()));
  auto luaRoot = LuaRoot::create(menu->context().root());
  if (!luaRoot) return std::unexpected(std::move(luaRoot.error()));
  if (auto set = menu->m_script.set_lua_root(std::move(*luaRoot)); !set)
    return std::unexpected(std::move(set.error()));
  auto voice = menu->context().voice();
  if (!voice) return std::unexpected(std::move(voice.error()));
  auto callbacks = LuaBindings::makeVoiceCallbacks(voice->get());
  if (!callbacks) return std::unexpected(std::move(callbacks.error()));
  if (auto added = menu->m_script.add_callbacks("voice", std::move(*callbacks)); !added)
    return std::unexpected(std::move(added.error()));
  return menu;
}

std::expected<void, StarException> VoiceSettingsMenu::show() {
  return BaseScriptPane::show();
}

std::expected<void, StarException> VoiceSettingsMenu::displayed() {
  return BaseScriptPane::displayed();
}

std::expected<void, StarException> VoiceSettingsMenu::dismissed() {
  return BaseScriptPane::dismissed();
}

}