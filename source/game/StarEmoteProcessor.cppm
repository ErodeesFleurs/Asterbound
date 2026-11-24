module;

import std;
import star.list;
import star.string;
import star.format;

namespace Star {
class Assets;
class Root;


// Keep the game-heavy Humanoid definition out of interface BMIs.
enum class HumanoidEmote;

class EmoteProcessor {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<EmoteProcessor>, StarException> create(std::shared_ptr<Assets const> const& assets);

  [[nodiscard]] std::expected<HumanoidEmote, StarException> detect_emotes(String const& chatter) const;
  [[nodiscard]] static std::expected<HumanoidEmote, StarException> detect_chat_emote(Root& root, String const& chatter);

private:
  EmoteProcessor() = default;
  struct EmoteBinding {
    float length_bonus;
    bool ascii;
    String text;
    HumanoidEmote emote{};
  };
  List<EmoteBinding> m_emote_bindings;
};

}

export module star.emote_processor;

export namespace Star {
  using ::Star::HumanoidEmote;
  using ::Star::EmoteProcessor;
}
