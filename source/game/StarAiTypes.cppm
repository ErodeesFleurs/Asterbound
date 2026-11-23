module;
import std;
import star.string;
import star.format;
import star.json;
import star.ordered_set;

namespace Star {

struct AiExceptionTag {
  static constexpr std::string_view name() { return "AiException"; }
};
using AiException = StarError<AiExceptionTag, StarException>;

struct AiSpeech {
  String animation;
  String text;
  float speed_modifier;
};

struct AiState {
  [[nodiscard]] static std::expected<AiState, StarException> create(Json const& config);

  Json to_json() const;

  OrderedHashSet<String> available_missions;
  OrderedHashSet<String> completed_missions;
};

struct AiSpeciesMissionText {
  String button_text;
  String repeat_button_text;
  AiSpeech select_speech;
};

struct AiMission {
  String mission_name;
  String mission_unique_world;
  std::optional<String> warp_animation;
  std::optional<bool> warp_deploy;
  String icon;
  StringMap<AiSpeciesMissionText> species_text;
};

}

export module star.ai_types;

export namespace Star {
  using ::Star::AiExceptionTag;
  using ::Star::AiException;
  using ::Star::AiSpeech;
  using ::Star::AiState;
  using ::Star::AiSpeciesMissionText;
  using ::Star::AiMission;
}
