module;
import std;
import star.format;
import star.type_identity;
import star.json;

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

namespace Star {

class TabSetWidget;
class ListWidget;

class KeybindingsMenu : public Pane {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<KeybindingsMenu, Pane>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<KeybindingsMenu>();
  }

  [[nodiscard]] static std::expected<std::shared_ptr<KeybindingsMenu>, StarException> create(GuiContext& guiContext);

  // We need to handle our own Esc dismissal
  [[nodiscard]] std::expected<KeyboardCaptureMode, StarException> keyboardCaptureMode() const override;
  [[nodiscard]] std::expected<bool, StarException> sendEvent(InputEvent const& event) override;

  [[nodiscard]] std::expected<void, StarException> show() override;
  [[nodiscard]] std::expected<void, StarException> dismissed() override;

private:
  explicit KeybindingsMenu(GuiContext& guiContext);
  [[nodiscard]] std::expected<void, StarException> init();
  [[nodiscard]] std::expected<void, StarException> buildListsFromConfig();
  [[nodiscard]] std::expected<void, StarException> activateBinding(Widget& widget);
  [[nodiscard]] std::expected<void, StarException> setKeybinding(KeyChord desc);
  [[nodiscard]] std::expected<void, StarException> clearActive();
  [[nodiscard]] std::expected<void, StarException> exitActiveMode();
  [[nodiscard]] std::expected<void, StarException> apply();
  [[nodiscard]] std::expected<void, StarException> revert();
  [[nodiscard]] std::expected<void, StarException> resetDefaults();

  std::weak_ptr<Widget> m_activeKeybinding;

  Map<std::weak_ptr<Widget>, InterfaceAction, std::owner_less<std::weak_ptr<Widget>>> m_childToAction;
  std::shared_ptr<TabSetWidget> m_tabSet;
  std::shared_ptr<ListWidget> m_playerList;
  std::shared_ptr<ListWidget> m_toolBarList;
  std::shared_ptr<ListWidget> m_gameList;

  Json m_origConfiguration;

  std::size_t m_maxBindings;
  KeyMod m_currentMods;
};

}

export module star.keybindings_menu;

export namespace Star {
  using ::Star::TabSetWidget;
  using ::Star::ListWidget;
  using ::Star::KeybindingsMenu;
}
