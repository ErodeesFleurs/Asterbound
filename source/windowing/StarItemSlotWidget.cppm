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
import star.progress_widget;
import star.animation;

import star.format;
namespace Star {

class Item;

class ItemSlotWidget : public Widget {
  struct ConstructionKey { explicit ConstructionKey() = default; };
public:
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<ItemSlotWidget, Widget>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<ItemSlotWidget>(); }

  ItemSlotWidget(ConstructionKey, GuiContext& context, std::shared_ptr<Item> const& item, String const& backing_image,
      Animation&& new_item_indicator, Animation&& highlight_animation);
  [[nodiscard]] static std::expected<std::shared_ptr<ItemSlotWidget>, StarException> create(GuiContext& context, std::shared_ptr<Item> const& item, String const& backing_image);

  [[nodiscard]] std::expected<void, StarException> update(float dt) override;
  [[nodiscard]] std::expected<bool, StarException> sendEvent(InputEvent const& event) override;
  void setCallback(WidgetCallbackFunc callback);
  void setRightClickCallback(WidgetCallbackFunc callback);
  void setMiddleClickCallback(WidgetCallbackFunc callback);
  void setItem(std::shared_ptr<Item> const& item);
  std::shared_ptr<Item> item() const;
  void setProgress(float progress);
  void setBackingImageAffinity(bool full, bool empty);
  void setCountPosition(TextPositioning textPositioning);
  void setCountFontMode(FontMode fontMode);

  void showDurability(bool show);
  void showCount(bool show);
  void showRarity(bool showRarity);
  void showLinkIndicator(bool showLinkIndicator);
  void showSecondaryIcon(bool show);

  void indicate_new();
  bool indicating_new() const noexcept;

  void set_highlight_enabled(bool highlight);

protected:
  [[nodiscard]] std::expected<void, StarException> renderImpl() override;

private:
  std::shared_ptr<Item> m_item;

  String m_backing_image;
  bool m_drawBackingImageWhenFull;
  bool m_drawBackingImageWhenEmpty;
  bool m_showDurability;
  bool m_showCount;
  bool m_showRarity;
  bool m_showLinkIndicator;
  bool m_showSecondaryIcon;

  TextPositioning m_countPosition;
  FontMode m_countFontMode;

  Vec2I m_durabilityOffset;
  RectI m_itemDraggableArea;

  TextStyle m_textStyle;

  WidgetCallbackFunc m_callback;
  WidgetCallbackFunc m_rightClickCallback;
  WidgetCallbackFunc m_middleClickCallback;
  float m_progress;

  std::shared_ptr<ProgressWidget> m_durabilityBar;

  bool m_new_item_indicator_active = false;
  Animation m_new_item_indicator;

  bool m_highlight_enabled = false;
  Animation m_highlight_animation;
};

}

export module star.item_slot_widget;

export namespace Star {
  using ::Star::ItemSlotWidget;
  using ::Star::Item;
}
