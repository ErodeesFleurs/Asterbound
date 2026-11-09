module;
import std;
import star.type_identity;
import star.format;
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
import star.rpc_promise;


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

class ConfirmationDialog : public Pane {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<ConfirmationDialog, Pane>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<ConfirmationDialog>();
  }

  [[nodiscard]] static std::expected<std::shared_ptr<ConfirmationDialog>, StarException> create(GuiContext& guiContext);

  ~ConfirmationDialog() override = default;

  [[nodiscard]] std::expected<void, StarException> displayConfirmation(Json const& dialogConfig, RpcPromiseKeeper<Json> resultPromise);
  [[nodiscard]] std::expected<void, StarException> displayConfirmation(Json const& dialogConfig, WidgetCallbackFunc okCallback, WidgetCallbackFunc cancelCallback);
  
  std::optional<EntityId> sourceEntityId();

  [[nodiscard]] std::expected<void, StarException> dismissed() override;

private:
  explicit ConfirmationDialog(GuiContext& guiContext);
  [[nodiscard]] std::expected<void, StarException> ok();

  WidgetCallbackFunc m_okCallback;
  WidgetCallbackFunc m_cancelCallback;
  bool m_confirmed;

  std::optional<EntityId> m_sourceEntityId;

  std::optional<RpcPromiseKeeper<Json>> m_resultPromise;
};

}

export module star.confirmation_dialog;

export namespace Star {
  using ::Star::ConfirmationDialog;
}
