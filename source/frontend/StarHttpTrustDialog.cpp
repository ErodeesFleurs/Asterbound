module;
import std;
import star.json;
import star.string;
import star.format;
import star.assets;
import star.widget_parsing;
import star.root_base;
import star.root;
import star.gui_context;
import star.widget;
import star.pane;
import star.gui_reader;
import star.label_widget;
import star.button_widget;

module star.http_trust_dialog;

namespace Star {

HttpTrustDialog::HttpTrustDialog(ConstructionKey, GuiContext& gui_context) : Pane(gui_context) {}

std::expected<std::shared_ptr<HttpTrustDialog>, StarException> HttpTrustDialog::create(GuiContext& gui_context) {
  auto pane = std::make_shared<HttpTrustDialog>(ConstructionKey{}, gui_context);
  auto initialized = pane->initializePane();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return pane;
}

std::expected<void, StarException> HttpTrustDialog::display_request(String const& domain, Reply reply) {
  if (!reply || reply_)
    return std::unexpected(StarException("HTTP trust dialog requires exactly one pending reply"));
  auto assets = context().root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  removeAllChildren();
  GuiReader reader(context());
  reader.registerCallback("yes", [&pane = *this](Widget&) { return pane.respond(true); });
  reader.registerCallback("no", [&pane = *this](Widget&) { return pane.respond(false); });
  auto config = (*assets)->json("/interface/httpwarning/warning.config");
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto layout = config->get("paneLayout");
  if (!layout)
    return std::unexpected(std::move(layout.error()));
  auto constructed = reader.construct(*layout, *this);
  if (!constructed)
    return std::unexpected(std::move(constructed.error()));
  auto label = requireChild<LabelWidget>("domain");
  if (!label)
    return std::unexpected(std::move(label.error()));
  auto checkbox = requireChild<ButtonWidget>("rememberCheckbox");
  if (!checkbox)
    return std::unexpected(std::move(checkbox.error()));
  (*checkbox)->setChecked(false);
  auto text = (*label)->setText(Star::format("^green;{}^reset;", domain));
  if (!text)
    return std::unexpected(std::move(text.error()));
  auto shown = show();
  if (shown)
    reply_ = std::move(reply);
  return shown;
}

std::expected<void, StarException> HttpTrustDialog::respond(bool allow) {
  auto reply = std::exchange(reply_, {});
  if (!reply)
    return {};
  auto checkbox = fetchChild<ButtonWidget>("rememberCheckbox");
  dismiss();
  if (!checkbox || !*checkbox)
    return reply(false, false);
  return reply(allow, allow && (*checkbox)->isChecked());
}

std::expected<void, StarException> HttpTrustDialog::dismissed() {
  auto reply = std::exchange(reply_, {});
  auto replied = reply ? reply(false, false) : std::expected<void, StarException>{};
  auto dismissed = Pane::dismissed();
  if (!replied)
    return std::unexpected(std::move(replied.error()));
  return dismissed;
}

}
