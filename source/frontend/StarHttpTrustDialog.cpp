module;
import std;
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
import star.string;
import star.text;
import star.asset_path;
import star.listener;
import star.game_types;
import star.set;
import star.audio;
import star.map;
import star.format;
import star.version;


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
import star.widget_parsing;
import star.gui_reader;
import star.label_widget;
import star.button_group;
import star.button_widget;
import star.configuration;

module star.http_trust_dialog;

namespace Star {

HttpTrustDialog::HttpTrustDialog(GuiContext& guiContext) : Pane(guiContext), m_confirmed(true) {}

HttpTrustDialog::~HttpTrustDialog() = default;

std::expected<std::shared_ptr<HttpTrustDialog>, StarException> HttpTrustDialog::create(GuiContext& guiContext) {
  auto pane = std::shared_ptr<HttpTrustDialog>(new HttpTrustDialog(guiContext));
  auto initialized = pane->initializePane();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return pane;
}

std::expected<void, StarException> HttpTrustDialog::displayRequest(String const& domain, std::function<std::expected<void, StarException>(HttpTrustReply, bool)> callback) {
  if (!callback)
    return std::unexpected(StarException("HTTP trust request has no reply callback"));
  auto assets = context().root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  removeAllChildren();
  GuiReader reader(context());
  m_domain = domain;
  m_callback = std::move(callback);
  reader.registerCallback("yes", [this](Widget&) { return reply(HttpTrustReply::Allow); });
  reader.registerCallback("no", [this](Widget&) { return reply(HttpTrustReply::Deny); });
  m_confirmed = false;
  auto config = (*assets)->json("/interface/httpwarning/warning.config");
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto layout = config->get("paneLayout");
  if (!layout)
    return std::unexpected(std::move(layout.error()));
  auto constructed = reader.construct(*layout, *this);
  if (!constructed)
    return std::unexpected(std::move(constructed.error()));
  auto domainLabel = fetchChild<LabelWidget>("domain");
  if (!domainLabel)
    return std::unexpected(std::move(domainLabel.error()));
  if (!*domainLabel)
    return std::unexpected(GUIBuilderException("Missing HTTP trust domain label"));
  auto text = (*domainLabel)->setText(Star::format("^green;{}^reset;", domain));
  if (!text)
    return std::unexpected(std::move(text.error()));
  auto yes = fetchChild<ButtonWidget>("yes");
  if (!yes)
    return std::unexpected(std::move(yes.error()));
  if (!*yes)
    return std::unexpected(GUIBuilderException("Missing HTTP trust allow button"));
  (*yes)->setText("✅");
  auto no = fetchChild<ButtonWidget>("no");
  if (!no)
    return std::unexpected(std::move(no.error()));
  if (!*no)
    return std::unexpected(GUIBuilderException("Missing HTTP trust deny button"));
  (*no)->setText("❌");
  return show();
}

std::expected<void, StarException> HttpTrustDialog::reply(HttpTrustReply replyType) {
  auto checkbox = fetchChild<ButtonWidget>("rememberCheckbox");
  if (!checkbox)
    return std::unexpected(std::move(checkbox.error()));
  if (!*checkbox)
    return std::unexpected(GUIBuilderException("Missing HTTP trust remember checkbox"));
  bool remember = (*checkbox)->isChecked();
  if (replyType == HttpTrustReply::Allow && remember) {
    auto config = context().root().configuration();
    if (!config)
      return std::unexpected(std::move(config.error()));
    auto sites = (*config)->getPath("safe.luaHttp.trustedSites");
    if (!sites)
      return std::unexpected(std::move(sites.error()));
    auto existing = sites->optArray();
    if (!existing)
      return std::unexpected(std::move(existing.error()));
    auto trustedSites = existing->value_or(JsonArray{});
    bool exists = false;
    for (auto const& site : trustedSites) {
      auto name = site.toString();
      if (!name)
        return std::unexpected(std::move(name.error()));
      if (*name == m_domain) {
        exists = true;
        break;
      }
    }
    if (!exists) {
      trustedSites.push_back(m_domain);
      auto saved = (*config)->setPath("safe.luaHttp.trustedSites", trustedSites);
      if (!saved)
        return std::unexpected(std::move(saved.error()));
    }
  }
  if (!m_callback)
    return std::unexpected(StarException("HTTP trust request has no reply callback"));
  auto replied = m_callback(replyType, remember);
  if (!replied)
    return std::unexpected(std::move(replied.error()));
  m_confirmed = true;
  dismiss();
  return {};
}

std::expected<void, StarException> HttpTrustDialog::dismissed() {
  std::expected<void, StarException> replied;
  if (!m_confirmed) {
    if (!m_callback)
      replied = std::unexpected(StarException("HTTP trust request has no reply callback"));
    else
      replied = m_callback(HttpTrustReply::Deny, false);
    if (replied)
      m_confirmed = true;
  }
  auto dismissed = Pane::dismissed();
  if (!replied)
    return std::unexpected(std::move(replied.error()));
  return dismissed;
}

}
