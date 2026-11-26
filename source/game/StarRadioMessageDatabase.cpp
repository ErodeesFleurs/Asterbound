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

extern std::expected<EnumMap<RadioMessageType>, StarException> const radio_message_type_names = EnumMap<RadioMessageType>::create({
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
  if (!radio_message_type_names)
    return std::unexpected(radio_message_type_names.error());
  auto defaults = (*assets)->json("/radiomessages.config:messageDefaults");
  if (!defaults)
    return std::unexpected((*assets)->with_asset_context(std::move(defaults.error()), "/radiomessages.config", "Loading radio message defaults"));
  auto result = std::shared_ptr<RadioMessageDatabase>(new RadioMessageDatabase(std::move(*defaults)));
  auto files = (*assets)->scan_extension("radiomessages");
  if (!files)
    return std::unexpected(std::move(files.error()));
  for (auto const& asset_file : files->get()) {
    auto const& file = asset_file.string();
    auto loaded = [&]() -> std::expected<void, StarException> {
      auto messages = (*assets)->json(file);
      if (!messages)
        return std::unexpected(std::move(messages.error()));
      auto entries = messages->iterate_object();
      if (!entries)
        return std::unexpected(std::move(entries.error()));
      for (auto const& pair : *entries) {
        if (result->m_radio_messages.contains(pair.first))
          return std::unexpected(RadioMessageDatabaseException(Star::format("Duplicate radiomessage {} in file {}", pair.first, file)));
        auto message = result->create_radio_message(pair.second, pair.first);
        if (!message)
          return std::unexpected(std::move(message.error()));
        result->m_radio_messages[pair.first] = std::move(*message);
      }
      return {};
    }();
    if (!loaded)
      return std::unexpected(RadioMessageDatabaseException(Star::format("Error loading radiomessages file {}", file), (*assets)->with_asset_context(std::move(loaded.error()), file, "Loading radio messages")));
  }
  return result;
}

std::expected<RadioMessage, StarException> RadioMessageDatabase::radio_message(String const& message_name) const {
  if (auto message = m_radio_messages.find_ref(message_name))
    return message->get();
  return std::unexpected(RadioMessageDatabaseException(Star::format("Unknown radio message {}", message_name)));
}

std::expected<RadioMessage, StarException> RadioMessageDatabase::create_radio_message(Json const& config, std::optional<String> const& message_id) const {
  if (config.is_type(Json::Type::String)) {
    auto name = config.to_string();
    if (!name)
      return std::unexpected(std::move(name.error()));
    return radio_message(*name);
  }
  if (!config.is_type(Json::Type::Object))
    return std::unexpected(RadioMessageDatabaseException("Invalid radio message specification; expected message name or configuration."));
  auto merged_config = json_merge(m_defaults, config);
  RadioMessage message;
  auto configured_id = merged_config.get_string("messageId", "");
  if (!configured_id)
    return std::unexpected(std::move(configured_id.error()));
  message.message_id = message_id.value_or(*configured_id);
  if (message.message_id.empty())
    return std::unexpected(RadioMessageDatabaseException("Custom radio messages must specify a messageId!"));
  auto type_name = merged_config.get_string("type");
  if (!type_name)
    return std::unexpected(std::move(type_name.error()));
  auto type = radio_message_type_names->find_left_ref(*type_name);
  if (!type)
    return std::unexpected(std::move(type.error()));
  if (!*type)
    return std::unexpected(RadioMessageDatabaseException(Star::format("Unknown radio message type '{}'", *type_name)));
  message.type = (*type)->get();
  // Mission messages may repeat when the mission is replayed.
  auto unique = message.type == RadioMessageType::Mission ? config.get_bool("unique", false) : merged_config.get_bool("unique");
  if (!unique)
    return std::unexpected(std::move(unique.error()));
  message.unique = *unique;
  auto important = merged_config.get_bool("important");
  if (!important)
    return std::unexpected(std::move(important.error()));
  message.important = *important;
  auto text = merged_config.get_string("text");
  if (!text)
    return std::unexpected(std::move(text.error()));
  message.text = std::move(*text);
  auto sender_name = merged_config.get_string("senderName");
  if (!sender_name)
    return std::unexpected(std::move(sender_name.error()));
  message.sender_name = std::move(*sender_name);
  auto portrait_image = merged_config.get_string("portraitImage");
  if (!portrait_image)
    return std::unexpected(std::move(portrait_image.error()));
  message.portrait_image = std::move(*portrait_image);
  auto portrait_frames = merged_config.get_int("portraitFrames");
  if (!portrait_frames)
    return std::unexpected(std::move(portrait_frames.error()));
  message.portrait_frames = *portrait_frames;
  auto portrait_speed = merged_config.get_float("portraitSpeed");
  if (!portrait_speed)
    return std::unexpected(std::move(portrait_speed.error()));
  message.portrait_speed = *portrait_speed;
  auto text_speed = merged_config.get_float("textSpeed");
  if (!text_speed)
    return std::unexpected(std::move(text_speed.error()));
  message.text_speed = *text_speed;
  auto persist_time = merged_config.get_float("persistTime");
  if (!persist_time)
    return std::unexpected(std::move(persist_time.error()));
  message.persist_time = *persist_time;
  auto chatter_sound = merged_config.get_string("chatterSound");
  if (!chatter_sound)
    return std::unexpected(std::move(chatter_sound.error()));
  message.chatter_sound = std::move(*chatter_sound);
  auto without_ai = config.erase("speciesAiMessage");
  if (!without_ai)
    return std::unexpected(std::move(without_ai.error()));
  auto merger = without_ai->erase("speciesMessage");
  if (!merger)
    return std::unexpected(std::move(merger.error()));
  for (auto const& [key, overrides] : {
           std::pair{std::string_view{"speciesAiMessage"}, std::ref(message.species_ai_message)},
           std::pair{std::string_view{"speciesMessage"}, std::ref(message.species_message)}}) {
    auto variants = merged_config.get(key, JsonObject()).and_then([](Json const& value) { return value.iterate_object(); });
    if (!variants)
      return std::unexpected(std::move(variants.error()));
    for (auto const& pair : *variants) {
      auto nested = create_radio_message(json_merge(*merger, pair.second), message_id);
      if (!nested)
        return std::unexpected(std::move(nested.error()));
      overrides.get().try_emplace(pair.first, std::move(*nested));
    }
  }
  if (message.portrait_frames <= 0)
    return std::unexpected(RadioMessageDatabaseException(Star::format("Invalid portraitFrames {} in radio message config!", message.portrait_frames)));
  return message;
}

}
