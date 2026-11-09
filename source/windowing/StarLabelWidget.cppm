module;
import std;
import star.type_identity;
import star.json;

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

class LabelWidget : public Widget {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<LabelWidget, Widget>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<LabelWidget>();
  }

  [[nodiscard]] static std::expected<std::shared_ptr<LabelWidget>, StarException> create(GuiContext& context, String text = {},
      Color const& color = Color::White,
      HorizontalAnchor const& hAnchor = HorizontalAnchor::LeftAnchor,
      VerticalAnchor const& vAnchor = VerticalAnchor::BottomAnchor,
      std::optional<unsigned> wrapWidth = {},
      std::optional<float> lineSpacing = {});

  String const& text() const;
  std::optional<unsigned> getTextCharLimit() const;
  [[nodiscard]] std::expected<void, StarException> setText(String newText);
  [[nodiscard]] std::expected<void, StarException> setFontSize(int fontSize);
  void setFontMode(FontMode fontMode);
  void setColor(Color newColor);
  [[nodiscard]] std::expected<void, StarException> setAnchor(HorizontalAnchor hAnchor, VerticalAnchor vAnchor);
  [[nodiscard]] std::expected<void, StarException> setWrapWidth(std::optional<unsigned> wrapWidth);
  [[nodiscard]] std::expected<void, StarException> setLineSpacing(std::optional<float> lineSpacing);
  [[nodiscard]] std::expected<void, StarException> setDirectives(String const& directives);
  [[nodiscard]] std::expected<void, StarException> setTextCharLimit(std::optional<unsigned> charLimit);
  [[nodiscard]] std::expected<void, StarException> setTextStyle(TextStyle const& style);
  [[nodiscard]] std::expected<void, StarException> setFont(String const& font);

  RectI relativeBoundRect() const override;

protected:
  RectI getScissorRect() const override;
  [[nodiscard]] std::expected<void, StarException> renderImpl() override;

private:
  LabelWidget(GuiContext& context, HorizontalAnchor hAnchor, VerticalAnchor vAnchor, std::optional<unsigned> wrapWidth);
  [[nodiscard]] std::expected<void, StarException> updateTextRegion();

  String m_text;
  TextStyle m_style;
  HorizontalAnchor m_hAnchor;
  VerticalAnchor m_vAnchor;
  std::optional<unsigned> m_wrapWidth;
  std::optional<float> m_lineSpacing;
  std::optional<unsigned> m_textCharLimit;
  RectI m_textRegion;
};

}

export module star.label_widget;

export namespace Star {
  using ::Star::LabelWidget;
}
