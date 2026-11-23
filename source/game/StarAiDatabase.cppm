module;
import std;
import star.ai_types;
import star.animation;
import star.format;
import star.json;
import star.map;
import star.string;

namespace Star {

class Root;

class AiDatabase {
  struct ConstructionKey { explicit ConstructionKey() = default; };

public:
  AiDatabase(ConstructionKey, Animation&& static_animation, Animation&& scanline_animation);
  [[nodiscard]] static std::expected<std::shared_ptr<AiDatabase>, StarException> create(Root& root);

  [[nodiscard]] std::expected<std::reference_wrapper<AiMission const>, StarException> mission(String const& mission_name) const;
  [[nodiscard]] AiSpeciesMissionText const& mission_text(AiMission const& mission, String const& species) const;

  [[nodiscard]] std::expected<AiSpeech, StarException> ship_status(unsigned ship_level) const;
  AiSpeech no_missions_speech() const;
  AiSpeech no_crew_speech() const;

  [[nodiscard]] std::expected<String, StarException> portrait_image(String const& species, String const& frame = "idle.0") const;
  [[nodiscard]] std::expected<Animation, StarException> animation(String const& species, String const& animation_name) const;
  [[nodiscard]] std::expected<Animation, StarException> static_animation(String const& species) const;
  Animation scanline_animation() const;

  float characters_per_second() const;
  String default_animation() const;

private:
  struct AiAnimationConfig {
    AiAnimationConfig(Animation&& static_animation, Animation&& scanline_animation)
      : static_animation(std::move(static_animation)), scanline_animation(std::move(scanline_animation)) {}

    StringMap<Animation> ai_animations;
    String default_animation;
    float characters_per_second;

    Animation static_animation;
    float static_opacity;

    Animation scanline_animation;
    float scanline_opacity;
  };

  struct AiSpeciesParameters {
    String ai_frames;
    String portrait_frames;
    String static_frames;
  };

  [[nodiscard]] static std::expected<AiSpeech, StarException> parse_speech(Json const& v);
  [[nodiscard]] static std::expected<AiSpeciesParameters, StarException> parse_species_parameters(Json const& vm);

  [[nodiscard]] static std::expected<AiSpeciesMissionText, StarException> parse_species_mission_text(Json const& vm);
  [[nodiscard]] static std::expected<AiMission, StarException> parse_mission(Json const& vm);

  StringMap<AiMission> m_missions;
  StringMap<AiSpeciesParameters> m_species_parameters;
  Map<unsigned, AiSpeech> m_ship_status;
  AiSpeech m_no_missions_speech;
  AiSpeech m_no_crew_speech;

  AiAnimationConfig m_animation_config;
};

}

export module star.ai_database;

export namespace Star {
  using ::Star::AiDatabase;
}
