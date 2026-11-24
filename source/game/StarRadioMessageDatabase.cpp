import star.json;
import star.bi_map;
import star.ordered_map;
import star.thread;
import star.io_device;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.optional;
import star.string;
import star.format;

import star.asset_source;
import star.assets;
import star.root;

import star.radio_message_database;

namespace Star {

extern std::expected<EnumMap<RadioMessageType>, StarException> const RadioMessageTypeNames = EnumMap<RadioMessageType>::create({
    {RadioMessageType::Generic, "generic"},
    {RadioMessageType::Mission, "mission"},
    {RadioMessageType::Quest, "quest"},
    {RadioMessageType::Tutorial, "tutorial"},
});

RadioMessageDatabase::RadioMessageDatabase(Json defaults) : m_defaults(std::move(defaults)) {}

std::expected<std::shared_ptr<RadioMessageDatabase>, StarException> RadioMessageDatabase::create(Root& root) {
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  if (!RadioMessageTypeNames)
    return std::unexpected(RadioMessageTypeNames.error());
  auto defaults = (*assets)->json("/radiomessages.config:messageDefaults");
  if (!defaults)
    return std::unexpected((*assets)->with_asset_context(std::move(defaults.error()), "/radiomessages.config", "Loading radio message defaults"));
  auto result = std::shared_ptr<RadioMessageDatabase>(new RadioMessageDatabase(std::move(*defaults)));
  auto files = (*assets)->scan_extension("radiomessages");
  if (!files)
    return std::unexpected(std::move(files.error()));
  for (auto const& assetFile : files->get()) {
    auto const& file = assetFile.string();
    auto loaded = [&]() -> std::expected<void, StarException> {
      auto messages = (*assets)->json(file);
      if (!messages)
        return std::unexpected(std::move(messages.error()));
      auto entries = messages->iterate_object();
      if (!entries)
        return std::unexpected(std::move(entries.error()));
      for (auto const& pair : *entries) {
        if (result->m_radioMessages.contains(pair.first))
          return std::unexpected(RadioMessageDatabaseException(Star::format("Duplicate radiomessage {} in file {}", pair.first, file)));
        auto message = result->createRadioMessage(pair.second, pair.first);
        if (!message)
          return std::unexpected(std::move(message.error()));
        result->m_radioMessages[pair.first] = std::move(*message);
      }
      return {};
    }();
    if (!loaded)
      return std::unexpected(RadioMessageDatabaseException(Star::format("Error loading radiomessages file {}", file), (*assets)->with_asset_context(std::move(loaded.error()), file, "Loading radio messages")));
  }
  return result;
}

std::expected<RadioMessage, StarException> RadioMessageDatabase::radioMessage(String const& messageName) const {
  if (auto message = m_radioMessages.find_ref(messageName))
    return message->get();
  return std::unexpected(RadioMessageDatabaseException(Star::format("Unknown radio message {}", messageName)));
}

std::expected<RadioMessage, StarException> RadioMessageDatabase::createRadioMessage(Json const& config, std::optional<String> const& messageId) const {
  if (config.is_type(Json::Type::String)) {
    auto name = config.to_string();
    if (!name)
      return std::unexpected(std::move(name.error()));
    return radioMessage(*name);
  }
  if (!config.is_type(Json::Type::Object))
    return std::unexpected(RadioMessageDatabaseException("Invalid radio message specification; expected message name or configuration."));
  auto mergedConfig = json_merge(m_defaults, config);
  RadioMessage message;
  auto configuredId = mergedConfig.get_string("messageId", "");
  if (!configuredId)
    return std::unexpected(std::move(configuredId.error()));
  message.messageId = messageId.value_or(*configuredId);
  if (message.messageId.empty())
    return std::unexpected(RadioMessageDatabaseException("Custom radio messages must specify a messageId!"));
  auto typeName = mergedConfig.get_string("type");
  if (!typeName)
    return std::unexpected(std::move(typeName.error()));
  auto type = RadioMessageTypeNames->find_left_ref(*typeName);
  if (!type)
    return std::unexpected(std::move(type.error()));
  if (!*type)
    return std::unexpected(RadioMessageDatabaseException(Star::format("Unknown radio message type '{}'", *typeName)));
  message.type = (*type)->get();
  // Mission messages may repeat when the mission is replayed.
  auto unique = message.type == RadioMessageType::Mission ? config.get_bool("unique", false) : mergedConfig.get_bool("unique");
  if (!unique)
    return std::unexpected(std::move(unique.error()));
  message.unique = *unique;
  auto important = mergedConfig.get_bool("important");
  if (!important)
    return std::unexpected(std::move(important.error()));
  message.important = *important;
  auto text = mergedConfig.get_string("text");
  if (!text)
    return std::unexpected(std::move(text.error()));
  message.text = std::move(*text);
  auto senderName = mergedConfig.get_string("senderName");
  if (!senderName)
    return std::unexpected(std::move(senderName.error()));
  message.senderName = std::move(*senderName);
  auto portraitImage = mergedConfig.get_string("portraitImage");
  if (!portraitImage)
    return std::unexpected(std::move(portraitImage.error()));
  message.portraitImage = std::move(*portraitImage);
  auto portraitFrames = mergedConfig.get_int("portraitFrames");
  if (!portraitFrames)
    return std::unexpected(std::move(portraitFrames.error()));
  message.portraitFrames = *portraitFrames;
  auto portraitSpeed = mergedConfig.get_float("portraitSpeed");
  if (!portraitSpeed)
    return std::unexpected(std::move(portraitSpeed.error()));
  message.portraitSpeed = *portraitSpeed;
  auto textSpeed = mergedConfig.get_float("textSpeed");
  if (!textSpeed)
    return std::unexpected(std::move(textSpeed.error()));
  message.textSpeed = *textSpeed;
  auto persistTime = mergedConfig.get_float("persistTime");
  if (!persistTime)
    return std::unexpected(std::move(persistTime.error()));
  message.persistTime = *persistTime;
  auto chatterSound = mergedConfig.get_string("chatterSound");
  if (!chatterSound)
    return std::unexpected(std::move(chatterSound.error()));
  message.chatterSound = std::move(*chatterSound);
  auto withoutAi = config.erase("speciesAiMessage");
  if (!withoutAi)
    return std::unexpected(std::move(withoutAi.error()));
  auto merger = withoutAi->erase("speciesMessage");
  if (!merger)
    return std::unexpected(std::move(merger.error()));
  for (auto const& key : std::initializer_list<String>{"speciesAiMessage", "speciesMessage"}) {
    auto variants =
      mergedConfig.get(key, JsonObject()).and_then([](Json const& value) { return value.iterate_object(); });
    if (!variants)
      return std::unexpected(std::move(variants.error()));
    for (auto const& pair : *variants) {
      auto nested = createRadioMessage(json_merge(*merger, pair.second), messageId);
      if (!nested)
        return std::unexpected(std::move(nested.error()));
      message.speciesMessage[pair.first] = std::move(*nested);
    }
  }
  if (message.portraitFrames <= 0)
    return std::unexpected(RadioMessageDatabaseException(Star::format("Invalid portraitFrames {} in radio message config!", message.portraitFrames)));
  return message;
}

}
