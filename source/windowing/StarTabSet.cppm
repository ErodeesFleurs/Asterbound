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

namespace Star {

class FlowLayout;
class StackWidget;

class TabBarWidget;

struct TabSetConfig {
  String tabButtonBaseImage;
  String tabButtonHoverImage;
  String tabButtonPressedImage;
  String tabButtonBaseImageSelected;
  String tabButtonHoverImageSelected;
  String tabButtonPressedImageSelected;
  Vec2I tabButtonPressedOffset;
  Vec2I tabButtonTextOffset;
  Vec2I tabButtonSpacing;
};

class TabSetWidget : public Widget {
public:
  void const* query_type(TypeKey key) const noexcept override { return query_type_of<TabSetWidget, Widget>(this, key); }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<TabSetWidget>(); }

  [[nodiscard]] static std::expected<std::shared_ptr<TabSetWidget>, StarException> create(GuiContext& context, TabSetConfig const& tabSetConfig);

  void setSize(Vec2I const& size) override;

  [[nodiscard]] std::expected<void, StarException> addTab(String const& widgetName, std::shared_ptr<Widget> widget, String const& title);

  std::size_t tabCount() const;
  [[nodiscard]] std::expected<void, StarException> tabSelect(std::size_t page);
  std::size_t selectedTab() const;

  // Callback is called when the tab changes
  void setCallback(WidgetCallbackFunc callback);

private:
  explicit TabSetWidget(GuiContext& context, TabSetConfig const& config);
  int m_tabHeight = 0;
  TabSetConfig m_tabSetConfig;
  std::shared_ptr<FlowLayout> m_tabBar;
  std::shared_ptr<StackWidget> m_stack;
  WidgetCallbackFunc m_callback;
  std::optional<std::size_t> m_lastSelected;
};

}

export module star.tab_set;

export namespace Star {
  using ::Star::TabSetConfig;
  using ::Star::TabSetWidget;
  using ::Star::FlowLayout;
  using ::Star::StackWidget;
  using ::Star::TabBarWidget;
}
