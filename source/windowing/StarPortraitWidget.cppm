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
import star.rect;
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

import star.format;
namespace Star {

class PortraitEntity;

class Player;

class PortraitWidget : public Widget {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<PortraitWidget, Widget>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<PortraitWidget>();
  }

  [[nodiscard]] static std::expected<std::shared_ptr<PortraitWidget>, StarException> create(GuiContext& context, std::shared_ptr<PortraitEntity> entity = {}, PortraitMode mode = PortraitMode::Full);
  ~PortraitWidget() override = default;

  [[nodiscard]] std::expected<void, StarException> setEntity(std::shared_ptr<PortraitEntity> entity);
  [[nodiscard]] std::expected<void, StarException> setMode(PortraitMode mode);
  [[nodiscard]] std::expected<void, StarException> setScale(float scale);
  [[nodiscard]] std::expected<void, StarException> setIconMode();
  void setRenderHumanoid(bool);
  [[nodiscard]] std::expected<bool, StarException> sendEvent(InputEvent const& event) override;

protected:
  RectI getScissorRect() const override;
  [[nodiscard]] std::expected<void, StarException> renderImpl() override;

private:
  PortraitWidget(GuiContext& context, std::shared_ptr<PortraitEntity> entity, PortraitMode mode);
  [[nodiscard]] std::expected<void, StarException> init();
  [[nodiscard]] std::expected<void, StarException> updateSize();

  std::shared_ptr<PortraitEntity> m_entity;
  PortraitMode m_portraitMode;
  AssetPath m_noEntityImageFull;
  AssetPath m_noEntityImagePart;
  float m_scale;

  bool m_renderHumanoid;
  bool m_iconMode;
  AssetPath m_iconImage;
  Vec2I m_iconOffset;
};

}

export module star.portrait_widget;

export namespace Star {
  using ::Star::PortraitWidget;
  using ::Star::Player;
  using ::Star::PortraitEntity;
}
