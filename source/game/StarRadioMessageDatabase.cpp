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
import star.version;

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

RadioMessageDatabase::RadioMessageDatabase(Root& root) : m_root(root) {}

std::expected<std::shared_ptr<RadioMessageDatabase>, StarException> RadioMessageDatabase::create(Root& root) {
  auto result = std::shared_ptr<RadioMessageDatabase>(new RadioMessageDatabase(root));
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto files = (*assets)->scanExtension("radiomessages");
  if (!files)
    return std::unexpected(std::move(files.error()));
  for (auto const& assetFile : files->get()) {
    auto const& file = assetFile.string();
    auto loaded = [&]() -> std::expected<void, StarException> {
      auto messages = (*assets)->json(file);
      if (!messages)
        return std::unexpected(std::move(messages.error()));
      auto entries = messages->iterateObject();
      if (!entries)
        return std::unexpected(std::move(entries.error()));
      for (auto const& pair : *entries) {
        if (result->m_radioMessages.contains(pair.first))
          return std::unexpected(RadioMessageDatabaseException(strf("Duplicate radiomessage {} in file {}", pair.first, file)));
        auto message = result->createRadioMessage(pair.second, pair.first);
        if (!message)
          return std::unexpected(std::move(message.error()));
        result->m_radioMessages[pair.first] = std::move(*message);
      }
      return {};
    }();
    if (!loaded)
      return std::unexpected(RadioMessageDatabaseException(strf("Error loading radiomessages file {}", file), (*assets)->withAssetContext(std::move(loaded.error()), file, "Loading radio messages")));
  }
  return result;
}

std::expected<RadioMessage, StarException> RadioMessageDatabase::radioMessage(String const& messageName) const {
  if (auto message = m_radioMessages.find_ref(messageName))
    return message->get();
  return std::unexpected(RadioMessageDatabaseException(strf("Unknown radio message {}", messageName)));
}

std::expected<RadioMessage, StarException> RadioMessageDatabase::createRadioMessage(Json const& config, std::optional<String> const& messageId) const {
  if (config.isType(Json::Type::String)) {
    auto name = config.toString();
    if (!name)
      return std::unexpected(std::move(name.error()));
    return radioMessage(*name);
  }
  if (!config.isType(Json::Type::Object))
    return std::unexpected(RadioMessageDatabaseException("Invalid radio message specification; expected message name or configuration."));
  if (!RadioMessageTypeNames)
    return std::unexpected(RadioMessageTypeNames.error());
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto defaults = (*assets)->json("/radiomessages.config:messageDefaults");
  if (!defaults)
    return std::unexpected((*assets)->withAssetContext(std::move(defaults.error()), "/radiomessages.config", "Loading radio message defaults"));
  auto mergedConfig = jsonMerge(*defaults, config);
  RadioMessage message;
  auto configuredId = mergedConfig.getString("messageId", "");
  if (!configuredId)
    return std::unexpected(std::move(configuredId.error()));
  message.messageId = messageId.value_or(*configuredId);
  if (message.messageId.empty())
    return std::unexpected(RadioMessageDatabaseException("Custom radio messages must specify a messageId!"));
  auto typeName = mergedConfig.getString("type");
  if (!typeName)
    return std::unexpected(std::move(typeName.error()));
  auto type = RadioMessageTypeNames->find_left_ref(*typeName);
  if (!type)
    return std::unexpected(std::move(type.error()));
  if (!*type)
    return std::unexpected(RadioMessageDatabaseException(strf("Unknown radio message type '{}'", *typeName)));
  message.type = (*type)->get();
  // Mission messages may repeat when the mission is replayed.
  auto unique = message.type == RadioMessageType::Mission ? config.getBool("unique", false) : mergedConfig.getBool("unique");
  if (!unique)
    return std::unexpected(std::move(unique.error()));
  message.unique = *unique;
  auto important = mergedConfig.getBool("important");
  if (!important)
    return std::unexpected(std::move(important.error()));
  message.important = *important;
  auto text = mergedConfig.getString("text");
  if (!text)
    return std::unexpected(std::move(text.error()));
  message.text = std::move(*text);
  auto senderName = mergedConfig.getString("senderName");
  if (!senderName)
    return std::unexpected(std::move(senderName.error()));
  message.senderName = std::move(*senderName);
  auto portraitImage = mergedConfig.getString("portraitImage");
  if (!portraitImage)
    return std::unexpected(std::move(portraitImage.error()));
  message.portraitImage = std::move(*portraitImage);
  auto portraitFrames = mergedConfig.getInt("portraitFrames");
  if (!portraitFrames)
    return std::unexpected(std::move(portraitFrames.error()));
  message.portraitFrames = *portraitFrames;
  auto portraitSpeed = mergedConfig.getFloat("portraitSpeed");
  if (!portraitSpeed)
    return std::unexpected(std::move(portraitSpeed.error()));
  message.portraitSpeed = *portraitSpeed;
  auto textSpeed = mergedConfig.getFloat("textSpeed");
  if (!textSpeed)
    return std::unexpected(std::move(textSpeed.error()));
  message.textSpeed = *textSpeed;
  auto persistTime = mergedConfig.getFloat("persistTime");
  if (!persistTime)
    return std::unexpected(std::move(persistTime.error()));
  message.persistTime = *persistTime;
  auto chatterSound = mergedConfig.getString("chatterSound");
  if (!chatterSound)
    return std::unexpected(std::move(chatterSound.error()));
  message.chatterSound = std::move(*chatterSound);
  auto withoutAi = config.eraseKey("speciesAiMessage");
  if (!withoutAi)
    return std::unexpected(std::move(withoutAi.error()));
  auto merger = withoutAi->eraseKey("speciesMessage");
  if (!merger)
    return std::unexpected(std::move(merger.error()));
  for (auto const& key : std::initializer_list<String>{"speciesAiMessage", "speciesMessage"}) {
    auto variants = mergedConfig.get(key, JsonObject()).and_then([](Json const& value) { return value.iterateObject(); });
    if (!variants)
      return std::unexpected(std::move(variants.error()));
    for (auto const& pair : *variants) {
      auto nested = createRadioMessage(jsonMerge(*merger, pair.second), messageId);
      if (!nested)
        return std::unexpected(std::move(nested.error()));
      message.speciesMessage[pair.first] = std::move(*nested);
    }
  }
  if (message.portraitFrames <= 0)
    return std::unexpected(RadioMessageDatabaseException(strf("Invalid portraitFrames {} in radio message config!", message.portraitFrames)));
  return message;
}

}
