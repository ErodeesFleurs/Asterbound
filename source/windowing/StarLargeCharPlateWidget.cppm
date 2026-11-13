module;
import std;
import star.type_identity;
import star.json;

import star.vector;
import star.casting;
import star.input_event;
import star.application;
import star.statistics_service;
import star.p2p_networking_service;
import star.user_generated_content_service;
import star.desktop_service;
import star.image;
import star.application_controller;
import star.color;
import star.font;
import star.bi_map;
import star.ref_ptr;
import star.renderer;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
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
import star.button_group;
import star.button_widget;
import star.portrait_widget;
import star.label_widget;

import star.format;
namespace Star {

class Player;

class LargeCharPlateWidget : public ButtonWidget {
public:
  void const* query_type(TypeKey key) const noexcept override {
    return query_type_of<LargeCharPlateWidget, ButtonWidget>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<LargeCharPlateWidget>(); }

  [[nodiscard]] static std::expected<std::shared_ptr<LargeCharPlateWidget>, StarException> create(GuiContext& context, WidgetCallbackFunc mainCallback, std::shared_ptr<Player> player = {});

  [[nodiscard]] std::expected<void, StarException> mouseOut() override;

  [[nodiscard]] std::expected<void, StarException> setPlayer(std::shared_ptr<Player> player = {});

  [[nodiscard]] std::expected<void, StarException> enableDelete(WidgetCallbackFunc const& callback);
  void disableDelete();

  [[nodiscard]] std::expected<bool, StarException> sendEvent(InputEvent const& event) override;

  [[nodiscard]] std::expected<void, StarException> update(float dt) override;

protected:
  [[nodiscard]] std::expected<void, StarException> renderImpl() override;

private:
  explicit LargeCharPlateWidget(GuiContext& context);
  std::shared_ptr<Player> m_player;
  Json m_config;

  std::shared_ptr<PortraitWidget> m_portrait;
  Vec2I m_portraitOffset;
  float m_portraitScale;

  String m_playerPlateHover;
  String m_noPlayerPlate;
  String m_noPlayerPlateHover;
  String m_playerPlate;

  std::shared_ptr<LabelWidget> m_playerName;
  std::shared_ptr<LabelWidget> m_playerPhrase;
  std::shared_ptr<LabelWidget> m_modeName;
  std::shared_ptr<LabelWidget> m_mode;

  std::shared_ptr<ButtonWidget> m_delete;

  Vec2I m_playerNameOffset;
  Vec2I m_playerPhraseOffset;
  Vec2I m_modeNameOffset;
  Vec2I m_modeOffset;
  Vec2I m_deleteOffset;

  String m_createCharText;
  Color m_createCharTextColor;

  Color m_regularTextColor;
  Color m_disabledTextColor;
};

}

export module star.large_char_plate_widget;

export namespace Star {
  using ::Star::LargeCharPlateWidget;
  using ::Star::Player;
}
