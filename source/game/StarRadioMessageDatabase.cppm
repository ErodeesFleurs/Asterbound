module;

import std;
import star.string;
import star.format;
import star.bi_map;
import star.json;

namespace Star {

class Root;

struct RadioMessageDatabaseExceptionTag {
  static constexpr std::string_view name() { return "RadioMessageDatabaseException"; }
};
using RadioMessageDatabaseException = StarError<RadioMessageDatabaseExceptionTag, StarException>;

enum class RadioMessageType { Generic, Mission, Quest, Tutorial };
extern std::expected<EnumMap<RadioMessageType>, StarException> const radio_message_type_names;

struct RadioMessage {
  // Break the default-construction trait cycle in the recursive StringMap members.
  RadioMessage() = default;

  String message_id;
  RadioMessageType type;
  bool unique;
  bool important;
  String text;
  String sender_name;
  String portrait_image;
  int portrait_frames;
  float portrait_speed;
  float text_speed;
  float persist_time;
  String chatter_sound;

  StringMap<RadioMessage> species_ai_message;
  StringMap<RadioMessage> species_message;
};

class RadioMessageDatabase {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<RadioMessageDatabase>, StarException> create(Root& root);

  [[nodiscard]] std::expected<RadioMessage, StarException> radio_message(String const& message_name) const;
  [[nodiscard]] std::expected<RadioMessage, StarException> create_radio_message(Json const& config, std::optional<String> const& unique_id = {}) const;

private:
  explicit RadioMessageDatabase(Json defaults);
  Json m_defaults;
  StringMap<RadioMessage> m_radio_messages;
};

}

export module star.radio_message_database;

export namespace Star {
  using ::Star::RadioMessageDatabaseExceptionTag;
  using ::Star::RadioMessageDatabaseException;
  using ::Star::RadioMessageType;
  using ::Star::radio_message_type_names;
  using ::Star::RadioMessage;
  using ::Star::RadioMessageDatabase;
}
