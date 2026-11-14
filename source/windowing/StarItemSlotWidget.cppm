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

inline constexpr float ItemIndicateNewTime = 1.5f;

class ItemSlotWidget : public Widget {
public:
  void const* query_type(TypeKey key) const noexcept override {
    return query_type_of<ItemSlotWidget, Widget>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<ItemSlotWidget>(); }

  [[nodiscard]] static std::expected<std::shared_ptr<ItemSlotWidget>, StarException> create(GuiContext& context, std::shared_ptr<Item> const& item, String const& backingImage);

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

  void indicateNew();

  void setHighlightEnabled(bool highlight);

protected:
  [[nodiscard]] std::expected<void, StarException> renderImpl() override;

private:
  ItemSlotWidget(GuiContext& context, std::shared_ptr<Item> const& item, String const& backingImage, Animation newItemIndicator, Animation highlightAnimation);
  std::shared_ptr<Item> m_item;

  String m_backingImage;
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

  Animation m_newItemIndicator;

  bool m_highlightEnabled;
  Animation m_highlightAnimation;
};

}

export module star.item_slot_widget;

export namespace Star {
  using ::Star::ItemSlotWidget;
  using ::Star::Item;
  using ::Star::ItemIndicateNewTime;
}
