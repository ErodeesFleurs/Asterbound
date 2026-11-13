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
import star.uuid;

namespace Star {

class PlayerStorage;
class Player;

class CharSelectionPane : public Pane {
public:
  void const* query_type(TypeKey key) const noexcept override {
    return query_type_of<CharSelectionPane, Pane>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<CharSelectionPane>(); }

  using CreateCharCallback = std::function<std::expected<void, StarException>()>;
  using SelectCharacterCallback = std::function<std::expected<void, StarException>(std::shared_ptr<Player> const&)>;
  using DeleteCharacterCallback = std::function<std::expected<void, StarException>(Uuid)>;

  [[nodiscard]] static std::expected<std::shared_ptr<CharSelectionPane>, StarException> create(GuiContext& guiContext, std::shared_ptr<PlayerStorage> playerStorage, CreateCharCallback createCallback,
      SelectCharacterCallback selectCallback, DeleteCharacterCallback deleteCallback);

  [[nodiscard]] std::expected<bool, StarException> sendEvent(InputEvent const& event) override;
  [[nodiscard]] std::expected<void, StarException> show() override;
  [[nodiscard]] std::expected<void, StarException> updateCharacterPlates();
  [[nodiscard]] std::expected<void, StarException> setReadOnly(bool readOnly);

private:
  CharSelectionPane(GuiContext& guiContext, std::shared_ptr<PlayerStorage> playerStorage, CreateCharCallback createCallback,
      SelectCharacterCallback selectCallback, DeleteCharacterCallback deleteCallback);
  [[nodiscard]] std::expected<void, StarException> init();
  [[nodiscard]] std::expected<void, StarException> shiftCharacters(int movement);
  [[nodiscard]] std::expected<void, StarException> selectCharacter(unsigned buttonIndex);

  std::shared_ptr<PlayerStorage> m_playerStorage;
  unsigned m_downScroll;
  String m_search;
  List<Uuid> m_filteredList;
  bool m_readOnly = false;

  CreateCharCallback m_createCallback;
  SelectCharacterCallback m_selectCallback;
  DeleteCharacterCallback m_deleteCallback;
};
}

export module star.char_selection;

export namespace Star {
  using ::Star::PlayerStorage;
  using ::Star::Player;
  using ::Star::CharSelectionPane;
}
