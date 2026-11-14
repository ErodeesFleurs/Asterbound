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

class LabelWidget;
class ButtonWidget;
class ListWidget;

class ModsMenu : public Pane {
public:
  void const* query_type(TypeKey key) const noexcept override { return query_type_of<ModsMenu, Pane>(this, key); }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<ModsMenu>(); }

  [[nodiscard]] static std::expected<std::shared_ptr<ModsMenu>, StarException> create(GuiContext& guiContext);

  [[nodiscard]] std::expected<void, StarException> update(float dt) override;

private:
  explicit ModsMenu(GuiContext& guiContext);
  [[nodiscard]] std::expected<void, StarException> init();
  [[nodiscard]] static std::expected<String, StarException> bestModName(JsonObject const& metadata, String const& sourcePath);

  [[nodiscard]] std::expected<void, StarException> openLink();
  [[nodiscard]] std::expected<void, StarException> openWorkshop();

  StringList m_assetsSources;

  std::shared_ptr<ListWidget> m_modList;
  std::shared_ptr<LabelWidget> m_modName;
  std::shared_ptr<LabelWidget> m_modAuthor;
  std::shared_ptr<LabelWidget> m_modVersion;
  std::shared_ptr<LabelWidget> m_modPath;
  std::shared_ptr<LabelWidget> m_modDescription;

  std::shared_ptr<ButtonWidget> m_linkButton;
  std::shared_ptr<ButtonWidget> m_copyLinkButton;
};

}

export module star.mods_menu;

export namespace Star {
  using ::Star::LabelWidget;
  using ::Star::ButtonWidget;
  using ::Star::ListWidget;
  using ::Star::ModsMenu;
}
