module;
import std;
import star.format;
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
import star.widget_parsing;
import star.gui_reader;

namespace Star {

class ListWidget : public Widget {
public:
  void const* query_type(TypeKey key) const noexcept override { return query_type_of<ListWidget, Widget>(this, key); }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<ListWidget>(); }

  [[nodiscard]] static std::expected<std::shared_ptr<ListWidget>, StarException> create(GuiContext& context, Json const& schema);
  [[nodiscard]] static std::expected<std::shared_ptr<ListWidget>, StarException> create(GuiContext& context);

  RectI relativeBoundRect() const override;

  // Callback is called when the selection changes
  void setCallback(WidgetCallbackFunc callback);

  [[nodiscard]] std::expected<bool, StarException> sendEvent(InputEvent const& event) override;
  [[nodiscard]] std::expected<void, StarException> setSchema(Json const& schema);
  [[nodiscard]] std::expected<std::shared_ptr<Widget>, StarException> constructWidget();
  [[nodiscard]] std::expected<std::shared_ptr<Widget>, StarException> addItem();
  [[nodiscard]] std::expected<std::shared_ptr<Widget>, StarException> addItem(std::size_t at);
  [[nodiscard]] std::expected<std::shared_ptr<Widget>, StarException> addItem(std::shared_ptr<Widget> existingItem);
  [[nodiscard]] std::expected<void, StarException> removeItem(std::size_t at);
  [[nodiscard]] std::expected<void, StarException> removeItem(std::shared_ptr<Widget> item);
  [[nodiscard]] std::expected<void, StarException> clear();
  std::size_t selectedItem() const;
  std::size_t itemPosition(std::shared_ptr<Widget> item) const;
  std::shared_ptr<Widget> itemAt(std::size_t n) const;
  std::shared_ptr<Widget> selectedWidget() const;
  List<std::shared_ptr<Widget>> const& list() const;
  std::size_t listSize() const;
  [[nodiscard]] std::expected<void, StarException> setEnabled(std::size_t pos, bool enabled);
  [[nodiscard]] std::expected<void, StarException> setHovered(std::size_t pos, bool hovered);
  [[nodiscard]] std::expected<void, StarException> setSelected(std::size_t pos);
  [[nodiscard]] std::expected<void, StarException> clearSelected();
  [[nodiscard]] std::expected<void, StarException> setSelectedWidget(std::shared_ptr<Widget> selected);

  void registerMemberCallback(String const& name, WidgetCallbackFunc const& callback);

  void setFillDown(bool fillDown);
  [[nodiscard]] std::expected<void, StarException> setColumns(std::uint64_t columns);

private:
  explicit ListWidget(GuiContext& context);
  [[nodiscard]] std::expected<void, StarException> setBackground(std::size_t pos, String const& image);
  void updateSizeAndPosition();

  Json m_schema;
  GuiReader m_reader;

  Set<std::size_t> m_disabledItems;
  std::size_t m_selectedItem;
  WidgetCallbackFunc m_callback;

  String m_selectedBG;
  String m_unselectedBG;
  String m_hoverBG;
  String m_disabledBG;
  Vec2I m_spacing;

  bool m_fillDown;
  std::uint64_t m_columns;
};

}

export module star.list_widget;

export namespace Star {
  using ::Star::ListWidget;
}
