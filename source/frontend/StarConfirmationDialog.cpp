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


import star.confirmation_dialog;
import star.widget_parsing;
import star.gui_reader;
import star.label_widget;
import star.button_group;
import star.button_widget;
import star.image_widget;

import star.algorithm;
namespace Star {

ConfirmationDialog::ConfirmationDialog(GuiContext& guiContext) : Pane(guiContext), m_confirmed(true) {}

std::expected<std::shared_ptr<ConfirmationDialog>, StarException> ConfirmationDialog::create(GuiContext& guiContext) {
  auto pane = std::shared_ptr<ConfirmationDialog>(new ConfirmationDialog(guiContext));
  auto initialized = pane->initializePane();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return pane;
}

std::expected<void, StarException> ConfirmationDialog::displayConfirmation(Json const& dialogConfig, RpcPromiseKeeper<Json> resultPromise) {
  m_resultPromise = std::move(resultPromise);
  return displayConfirmation(dialogConfig,
    [this](Widget&) { return m_resultPromise->fulfill(true); },
    [this](Widget&) { return m_resultPromise->fulfill(false); });
}

std::expected<void, StarException> ConfirmationDialog::displayConfirmation(Json const& dialogConfig, WidgetCallbackFunc okCallback, WidgetCallbackFunc cancelCallback) {
  auto assets = context().root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  Json config = dialogConfig;
  if (dialogConfig.isType(Json::Type::String)) {
    auto path = dialogConfig.toString();
    if (!path)
      return std::unexpected(std::move(path.error()));
    auto loaded = (*assets)->json(*path);
    if (!loaded)
      return std::unexpected(std::move(loaded.error()));
    config = std::move(*loaded);
  }
  removeAllChildren();
  GuiReader reader(context());
  m_okCallback = std::move(okCallback);
  m_cancelCallback = std::move(cancelCallback);
  reader.registerCallback("close", [this](Widget&) -> std::expected<void, StarException> { dismiss(); return {}; });
  reader.registerCallback("cancel", [this](Widget&) -> std::expected<void, StarException> { dismiss(); return {}; });
  reader.registerCallback("ok", [this](Widget&) { return ok(); });
  m_confirmed = false;
  auto layoutPath = config.optString("paneLayout");
  if (!layoutPath)
    return std::unexpected(std::move(layoutPath.error()));
  auto layout = (*assets)->json(layoutPath->value_or("/interface/windowconfig/confirmation.config:paneLayout"));
  if (!layout)
    return std::unexpected(std::move(layout.error()));
  auto constructed = reader.construct(*layout, *this);
  if (!constructed)
    return std::unexpected(std::move(constructed.error()));
  std::shared_ptr<ImageWidget> titleIcon;
  auto icon = config.optString("icon");
  if (!icon)
    return std::unexpected(std::move(icon.error()));
  if (*icon) {
    auto created = ImageWidget::create(context(), **icon);
    if (!created)
      return std::unexpected(std::move(created.error()));
    titleIcon = std::move(*created);
  }
  auto title = config.getString("title", "");
  if (!title)
    return std::unexpected(std::move(title.error()));
  auto subtitle = config.getString("subtitle", "");
  if (!subtitle)
    return std::unexpected(std::move(subtitle.error()));
  auto titled = setTitle(titleIcon, *title, *subtitle);
  if (!titled)
    return std::unexpected(std::move(titled.error()));
  auto message = config.getString("message");
  if (!message)
    return std::unexpected(std::move(message.error()));
  auto label = fetchChild<LabelWidget>("message");
  if (!label)
    return std::unexpected(std::move(label.error()));
  if (!*label)
    return std::unexpected(GUIBuilderException("Missing confirmation message label"));
  auto text = (*label)->setText(*message);
  if (!text)
    return std::unexpected(std::move(text.error()));
  auto okCaption = config.optString("okCaption");
  if (!okCaption)
    return std::unexpected(std::move(okCaption.error()));
  if (*okCaption) {
    auto button = fetchChild<ButtonWidget>("ok");
    if (!button)
      return std::unexpected(std::move(button.error()));
    if (!*button)
      return std::unexpected(GUIBuilderException("Missing confirmation OK button"));
    (*button)->setText(**okCaption);
  }
  auto cancelCaption = config.optString("cancelCaption");
  if (!cancelCaption)
    return std::unexpected(std::move(cancelCaption.error()));
  if (*cancelCaption) {
    auto button = fetchChild<ButtonWidget>("cancel");
    if (!button)
      return std::unexpected(std::move(button.error()));
    if (!*button)
      return std::unexpected(GUIBuilderException("Missing confirmation cancel button"));
    (*button)->setText(**cancelCaption);
  }
  auto source = config.optInt("sourceEntityId");
  if (!source)
    return std::unexpected(std::move(source.error()));
  m_sourceEntityId = *source;
  auto images = config.optObject("images");
  if (!images)
    return std::unexpected(std::move(images.error()));
  if (*images) {
    for (auto const& image : **images) {
      auto widget = fetchChild<ImageWidget>(image.first);
      if (!widget)
        return std::unexpected(std::move(widget.error()));
      if (!*widget)
        return std::unexpected(GUIBuilderException::format("Missing confirmation image '{}'", image.first));
      if (image.second.isType(Json::Type::String)) {
        auto path = image.second.toString();
        if (!path)
          return std::unexpected(std::move(path.error()));
        auto set = (*widget)->setImage(*path);
        if (!set)
          return std::unexpected(std::move(set.error()));
      } else {
        auto array = image.second.toArray();
        if (!array)
          return std::unexpected(std::move(array.error()));
        auto metadata = context().root().imageMetadataDatabase();
        if (!metadata)
          return std::unexpected(std::move(metadata.error()));
        List<Drawable> drawables;
        for (auto const& entry : *array) {
          auto drawable = Drawable::create(**metadata, entry);
          if (!drawable)
            return std::unexpected(std::move(drawable.error()));
          drawables.push_back(std::move(*drawable));
        }
        auto set = (*widget)->setDrawables(std::move(drawables));
        if (!set)
          return std::unexpected(std::move(set.error()));
      }
    }
  }
  auto labels = config.optObject("labels");
  if (!labels)
    return std::unexpected(std::move(labels.error()));
  if (*labels) {
    for (auto const& entry : **labels) {
      auto widget = fetchChild<LabelWidget>(entry.first);
      if (!widget)
        return std::unexpected(std::move(widget.error()));
      if (!*widget)
        return std::unexpected(GUIBuilderException::format("Missing confirmation label '{}'", entry.first));
      auto value = entry.second.toString();
      if (!value)
        return std::unexpected(std::move(value.error()));
      auto set = (*widget)->setText(*value);
      if (!set)
        return std::unexpected(std::move(set.error()));
    }
  }
  auto shown = show();
  if (!shown)
    return std::unexpected(std::move(shown.error()));
  auto soundsJson = (*assets)->json("/interface/windowconfig/confirmation.config:onShowSound");
  if (!soundsJson)
    return std::unexpected(std::move(soundsJson.error()));
  auto sounds = soundsJson->toArray();
  if (!sounds)
    return std::unexpected(std::move(sounds.error()));
  auto sound = context().root().random().randValueFrom(*sounds, "").toString();
  if (!sound)
    return std::unexpected(std::move(sound.error()));
  if (!sound->empty()) {
    auto played = context().playAudio(*sound);
    if (!played)
      return std::unexpected(std::move(played.error()));
  }
  return {};
}

std::optional<EntityId> ConfirmationDialog::sourceEntityId() {
  return m_sourceEntityId;
}

std::expected<void, StarException> ConfirmationDialog::dismissed() {
  std::expected<void, StarException> cancelled;
  if (!m_confirmed) {
    if (!m_cancelCallback)
      cancelled = std::unexpected(StarException("Confirmation dialog has no cancellation callback"));
    else
      cancelled = m_cancelCallback(*this);
    if (cancelled)
      m_confirmed = true;
  }
  auto dismissed = Pane::dismissed();
  if (!cancelled)
    return std::unexpected(std::move(cancelled.error()));
  return dismissed;
}

std::expected<void, StarException> ConfirmationDialog::ok() {
  if (!m_okCallback)
    return std::unexpected(StarException("Confirmation dialog has no confirmation callback"));
  auto confirmed = m_okCallback(*this);
  if (!confirmed)
    return std::unexpected(std::move(confirmed.error()));
  m_confirmed = true;
  dismiss();
  return {};
}

}
