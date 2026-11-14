module;
import std;
import star.type_identity;
import star.format;
import star.json;
import star.casting;
import star.input_event;
import star.color;
import star.font;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.bi_map;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.version;
import star.string;
import star.text;
import star.asset_path;
import star.listener;
import star.game_types;
import star.set;
import star.audio;
import star.map;


import star.application;
import star.statistics_service;
import star.p2p_networking_service;
import star.user_generated_content_service;
import star.desktop_service;
import star.image;
import star.application_controller;
import star.renderer;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
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

export module star.http_trust_dialog;

export namespace Star {

enum class HttpTrustReply {
  Allow,
  Deny
};

class HttpTrustDialog final : public Pane {
public:
  void const* query_type(TypeKey key) const noexcept override {
    return query_type_of<HttpTrustDialog, Pane>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<HttpTrustDialog>(); }

  [[nodiscard]] static std::expected<std::shared_ptr<HttpTrustDialog>, StarException> create(GuiContext& guiContext);

  ~HttpTrustDialog() override;

  [[nodiscard]] std::expected<void, StarException> displayRequest(String const& domain, std::function<std::expected<void, StarException>(HttpTrustReply, bool)> callback);

  [[nodiscard]] std::expected<void, StarException> dismissed() override;

private:
  explicit HttpTrustDialog(GuiContext& guiContext);
  [[nodiscard]] std::expected<void, StarException> reply(HttpTrustReply replyType);

  String m_domain;
  bool m_confirmed;
  std::function<std::expected<void, StarException>(HttpTrustReply, bool)> m_callback;
};

}
