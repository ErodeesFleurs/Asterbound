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
extern std::expected<EnumMap<RadioMessageType>, StarException> const RadioMessageTypeNames;

struct RadioMessage {
  // Break the default-construction trait cycle in the recursive StringMap members.
  RadioMessage() = default;

  String messageId;
  RadioMessageType type;
  bool unique;
  bool important;
  String text;
  String senderName;
  String portraitImage;
  int portraitFrames;
  float portraitSpeed;
  float textSpeed;
  float persistTime;
  String chatterSound;

  StringMap<RadioMessage> speciesAiMessage;
  StringMap<RadioMessage> speciesMessage;
};

class RadioMessageDatabase {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<RadioMessageDatabase>, StarException> create(Root& root);

  [[nodiscard]] std::expected<RadioMessage, StarException> radioMessage(String const& messageName) const;
  [[nodiscard]] std::expected<RadioMessage, StarException> createRadioMessage(Json const& config, std::optional<String> const& uniqueId = {}) const;

private:
  explicit RadioMessageDatabase(Json defaults);
  Json m_defaults;
  StringMap<RadioMessage> m_radioMessages;
};

}

export module star.radio_message_database;

export namespace Star {
  using ::Star::RadioMessageDatabaseExceptionTag;
  using ::Star::RadioMessageDatabaseException;
  using ::Star::RadioMessageType;
  using ::Star::RadioMessageTypeNames;
  using ::Star::RadioMessage;
  using ::Star::RadioMessageDatabase;
}
