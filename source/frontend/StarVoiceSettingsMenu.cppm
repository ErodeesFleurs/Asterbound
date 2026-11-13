module;
import std;
import star.type_identity;
import star.json;
import star.format;

import star.casting;
import star.input_event;
import star.color;
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
import star.game_types;
import star.set;
import star.audio;
import star.map;
import star.lua_components;




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

namespace Star {

class VoiceSettingsMenu : public BaseScriptPane {
public:
  void const* query_type(TypeKey key) const noexcept override {
    return query_type_of<VoiceSettingsMenu, BaseScriptPane>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<VoiceSettingsMenu>(); }

  [[nodiscard]] static std::expected<std::shared_ptr<VoiceSettingsMenu>, StarException> create(GuiContext& guiContext, Json const& config);

  [[nodiscard]] std::expected<void, StarException> show() override;
  [[nodiscard]] std::expected<void, StarException> displayed() override;
  [[nodiscard]] std::expected<void, StarException> dismissed() override;

private:
  VoiceSettingsMenu(GuiContext& guiContext);

};

}

export module star.voice_settings_menu;

export namespace Star {
  using ::Star::VoiceSettingsMenu;
}
