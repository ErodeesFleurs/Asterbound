import std;
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
import star.rpc_promise;
import star.random;


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


import star.join_request_dialog;
import star.widget_parsing;
import star.gui_reader;
import star.label_widget;
import star.button_group;
import star.button_widget;
import star.image_widget;

namespace Star {

JoinRequestDialog::JoinRequestDialog(GuiContext& guiContext) : Pane(guiContext), m_confirmed(true) {}

std::expected<std::shared_ptr<JoinRequestDialog>, StarException> JoinRequestDialog::create(GuiContext& guiContext) {
  auto pane = std::shared_ptr<JoinRequestDialog>(new JoinRequestDialog(guiContext));
  auto initialized = pane->initializePane();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return pane;
}

std::expected<void, StarException> JoinRequestDialog::displayRequest(String const& userName, std::function<std::expected<void, StarException>(P2PJoinRequestReply)> callback) {
  if (!callback)
    return std::unexpected(StarException("Join request has no reply callback"));
  auto assets = context().root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  removeAllChildren();
  GuiReader reader(context());
  m_callback = std::move(callback);
  reader.registerCallback("yes", [this](Widget&) { return reply(P2PJoinRequestReply::Yes); });
  reader.registerCallback("no", [this](Widget&) { return reply(P2PJoinRequestReply::No); });
  reader.registerCallback("ignore", [this](Widget&) { return reply(P2PJoinRequestReply::Ignore); });
  m_confirmed = false;
  auto config = (*assets)->json("/interface/windowconfig/joinrequest.config");
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto layout = config->get("paneLayout");
  if (!layout)
    return std::unexpected(std::move(layout.error()));
  auto constructed = reader.construct(*layout, *this);
  if (!constructed)
    return std::unexpected(std::move(constructed.error()));
  auto message = config->getString("joinMessage");
  if (!message)
    return std::unexpected(std::move(message.error()));
  auto label = fetchChild<LabelWidget>("message");
  if (!label)
    return std::unexpected(std::move(label.error()));
  if (!*label)
    return std::unexpected(GUIBuilderException("Missing join request message label"));
  auto text = (*label)->setText(message->replace_tags(StringMap<String>{{"username", userName}}));
  if (!text)
    return std::unexpected(std::move(text.error()));
  return show();
}

std::expected<void, StarException> JoinRequestDialog::reply(P2PJoinRequestReply reply) {
  if (!m_callback)
    return std::unexpected(StarException("Join request has no reply callback"));
  auto replied = m_callback(reply);
  if (!replied)
    return std::unexpected(std::move(replied.error()));
  m_confirmed = true;
  dismiss();
  return {};
}

std::expected<void, StarException> JoinRequestDialog::dismissed() {
  std::expected<void, StarException> replied;
  if (!m_confirmed) {
    if (!m_callback)
      replied = std::unexpected(StarException("Join request has no reply callback"));
    else
      replied = m_callback(P2PJoinRequestReply::No);
    if (replied)
      m_confirmed = true;
  }
  auto dismissed = Pane::dismissed();
  if (!replied)
    return std::unexpected(std::move(replied.error()));
  return dismissed;
}

}
