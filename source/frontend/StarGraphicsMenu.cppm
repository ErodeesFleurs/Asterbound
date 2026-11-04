module;
import std;
import star.format;
import star.type_identity;
import star.json;

import star.vector;
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

import star.ordered_map;




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
import star.game_timers;
import star.pane_manager;
import star.registered_pane_manager;
import star.main_interface_types;

namespace Star {
class UniverseClient;
}

namespace Star {

class ShadersMenu;

class GraphicsMenu : public Pane {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<GraphicsMenu, Pane>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<GraphicsMenu>();
  }

  [[nodiscard]] static std::expected<std::shared_ptr<GraphicsMenu>, StarException> create(GuiContext& guiContext, PaneManager& manager, std::shared_ptr<UniverseClient> client);
  void detachInterface() noexcept;

  [[nodiscard]] std::expected<void, StarException> show() override;
  [[nodiscard]] std::expected<void, StarException> dismissed() override;

  [[nodiscard]] std::expected<void, StarException> toggleFullscreen();

private:
  GraphicsMenu(GuiContext& guiContext, PaneManager& manager);
  [[nodiscard]] std::expected<void, StarException> init(std::shared_ptr<UniverseClient> client);
  static StringList const ConfigKeys;

  [[nodiscard]] std::expected<void, StarException> initConfig();
  enum class SettingUpdate { Staged, Live };
  [[nodiscard]] std::expected<void, StarException> updateFloatSlider(String const& name, String const& key,
                                                                     List<float> const& values, SettingUpdate update);
  [[nodiscard]] std::expected<void, StarException> applyLiveSetting(String const& key, Json const& value);
  [[nodiscard]] std::expected<void, StarException> syncGui();

  [[nodiscard]] std::expected<void, StarException> apply();
  [[nodiscard]] std::expected<void, StarException> applyWindowSettings();

  [[nodiscard]] std::expected<void, StarException> displayShaders();

  List<Vec2U> m_resList;
  List<float> m_interfaceScaleList;
  List<float> m_zoomList;
  List<float> m_cameraSpeedList;
  List<double> m_renderFpsPresets;
  List<double> m_renderFpsList;

  JsonObject m_localChanges;
  
  std::shared_ptr<ShadersMenu> m_shadersMenu;
  std::optional<std::reference_wrapper<PaneManager>> m_paneManager;
};

}

export module star.graphics_menu;

export namespace Star {
  using ::Star::GraphicsMenu;
  using ::Star::ShadersMenu;
}
